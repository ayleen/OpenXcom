#ifdef __EMSCRIPTEN__
/*
 * Phase 41 (Calypso) -- CalypsoPrologueScene implementation.
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 *
 * NOTE (Phase 39 gotcha): the `Log` macro cannot be namespace-qualified inside
 * src/Calypso/ files -- it is used bare here.
 *
 * All pure decision math (target selection, ambush predicate, escalation
 * staging) lives in CalypsoPrologueMath.h and is doctest-covered; this file
 * only wires that math to real BattleUnits/BattlescapeGame state.
 */

#include <algorithm>
#include <string>
#include <vector>

#include "CalypsoPrologueScene.h"
#include "CalypsoPrologueMath.h"

#include "../Battlescape/BattlescapeGame.h"
#include "../Battlescape/BattlescapeState.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/BattleUnit.h"
#include "../Mod/Unit.h"          // UnitFaction, SpecialTileType
#include "../Mod/RuleDamageType.h"
#include "../Mod/Mod.h"
#include "../Engine/RNG.h"
#include "../Engine/Game.h"       // getCurrentGame()
#include "../Engine/Logger.h"
#include "../Engine/Yaml.h"

namespace OpenXcom
{

// --------------------------------------------------------------------------- //
// tunables -- placeholders until the real map is assembled and measured
// (phase plan 41.1a step 5); every use site below is marked TUNE(41.1a step 5).
// --------------------------------------------------------------------------- //

// Craft (Nereid) exit/deployment zone, in tile coordinates. NE corner of the
// port per the design doc's map plan (docs/tutorial-mission-design.md §5).
static const Calypso::Rect EXIT_AREA{ 27, 2, 29, 4 };
static const Position EXIT_AREA_CENTER(28, 3, 0);

// Distance (Chebyshev tiles) the Assessor + one other unit must clear from the
// Nereid before the ambush can trigger -- "~two full TU sprints" (41.1a).
static const int TRIGGER_DIST = 8;
// Unconditional ambush turn if the distance trigger never fires (sabotage fallback).
static const int FALLBACK_TURN = 8;
// First player turn an escalating nag radio line can fire (see escalationStage()).
static const int FIRST_NAG_TURN = 3;
// Repeat-fire shots at one victim before the direct-damage fallback kicks in.
static const int SHOT_CAP_PER_TURN = 4;

// Herder waypoints: pen -> squad midpoint -> the Nereid itself. A single
// midpoint is enough for commit 3; the real map may need more via 41.1a.
static const Position HERDER_WAYPOINT_MID(15, 15, 0);

// Reserved for the map-tuning pass: alternate marksman perches used if the
// primary trajectory is blocked. directedShot() does not pre-validate the
// line in this commit (see CalypsoDirector.cpp notes); wiring a perch switch
// is deferred to when the real map exists to validate trajectories against.
[[maybe_unused]] static const Position MARKSMAN_PERCH_A(5, 5, 0);
[[maybe_unused]] static const Position MARKSMAN_PERCH_B(5, 25, 0);

static const char *STR_PROLOGUE_RADIO_LANDING   = "STR_PROLOGUE_RADIO_LANDING";
static const char *STR_PROLOGUE_RADIO_OBJECTIVE = "STR_PROLOGUE_RADIO_OBJECTIVE";
static const char *STR_PROLOGUE_RADIO_NAG_1     = "STR_PROLOGUE_RADIO_NAG_1";
static const char *STR_PROLOGUE_RADIO_NAG_2     = "STR_PROLOGUE_RADIO_NAG_2";
static const char *STR_PROLOGUE_RADIO_NAG_3     = "STR_PROLOGUE_RADIO_NAG_3";
static const char *STR_PROLOGUE_RADIO_AMBUSH    = "STR_PROLOGUE_RADIO_AMBUSH";
static const char *STR_PROLOGUE_LEADER_NAME     = "STR_PROLOGUE_LEADER_NAME";
static const char *STR_PROLOGUE_DIVER_LINE      = "STR_PROLOGUE_DIVER_LINE";
static const char *STR_PROLOGUE_NIKOS_LINE      = "STR_PROLOGUE_NIKOS_LINE";
static const char *STR_PROLOGUE_RADIO_SILENCE   = "STR_PROLOGUE_RADIO_SILENCE";

// --------------------------------------------------------------------------- //
// actor resolution
// --------------------------------------------------------------------------- //

bool CalypsoPrologueScene::resolveActors(BattlescapeGame *bg)
{
	SavedBattleGame *save = bg ? bg->getSave() : nullptr;
	if (!save) return false;

	std::vector<int> playerIds;
	for (BattleUnit *u : *save->getUnits())
	{
		if (!u || u->isOut()) continue;
		const std::string &type = u->getType();
		if (type == "STR_PROLOGUE_NIKOS")         _nikosId = u->getId();
		else if (type == "STR_PROLOGUE_ASSESSOR") _assessorId = u->getId();
		else if (type == "STR_PROLOGUE_HERDER")   _herderIds.push_back(u->getId());
		else if (type == "STR_PROLOGUE_MARKSMAN") _marksmanId = u->getId();
		else if (u->getFaction() == FACTION_PLAYER) playerIds.push_back(u->getId());
	}
	std::sort(playerIds.begin(), playerIds.end());

	bool ok = _nikosId >= 0 && _assessorId >= 0 && _marksmanId >= 0
		&& _herderIds.size() == 2 && playerIds.size() >= 3;
	if (!ok)
	{
		Log(LOG_ERROR) << "[prologue] actor resolution FAILED -- scene going inert. "
			<< "nikos=" << _nikosId << " assessor=" << _assessorId
			<< " marksman=" << _marksmanId << " herders=" << _herderIds.size()
			<< " players=" << playerIds.size();
		return false;
	}

	// Leader = lowest-id player soldier (commit 4 aligns roster spawn order so
	// this is the Expedition Leader); the other two are the divers.
	_leaderId = playerIds[0];
	_diverIds.assign(playerIds.begin() + 1, playerIds.begin() + 3);
	return true;
}

BattleUnit *CalypsoPrologueScene::findUnit(SavedBattleGame *save, int id)
{
	if (!save || id < 0) return nullptr;
	for (BattleUnit *u : *save->getUnits())
	{
		if (u && u->getId() == id) return u;
	}
	return nullptr;
}

std::vector<int> CalypsoPrologueScene::allPlayerIds() const
{
	std::vector<int> ids;
	ids.push_back(_leaderId);
	ids.insert(ids.end(), _diverIds.begin(), _diverIds.end());
	return ids;
}

int CalypsoPrologueScene::activeHerderId() const
{
	if (_activeHerderIdx < 0 || _activeHerderIdx >= (int)_herderIds.size()) return -1;
	return _herderIds[_activeHerderIdx];
}

// --------------------------------------------------------------------------- //
// lifecycle
// --------------------------------------------------------------------------- //

void CalypsoPrologueScene::onBattleStart(BattlescapeGame *bg)
{
	if (!resolveActors(bg))
	{
		_inert = true;
		return;
	}
	// D2: rolled once, drives which pattern the first post-Assessor death uses.
	_leaderDiesFirst = RNG::percent(50);
	_phase = Ph::MoveToOffice;
	radio(STR_PROLOGUE_RADIO_LANDING);
}

void CalypsoPrologueScene::onPlayerTurnStart(BattlescapeGame *bg)
{
	if (_inert || !bg) return;
	SavedBattleGame *save = bg->getSave();
	if (!save) return;

	// Panic-driven loss of control would fight the direction (41.3).
	CalypsoDirector::get().pinMorale(save, FACTION_PLAYER);

	if (_phase == Ph::MoveToOffice)
	{
		int stage = Calypso::escalationStage(save->getTurn(), FIRST_NAG_TURN, FALLBACK_TURN);
		if (stage >= 0 && stage != _lastNagStage)
		{
			_lastNagStage = stage;
			static const char *nagIds[3] = { STR_PROLOGUE_RADIO_NAG_1, STR_PROLOGUE_RADIO_NAG_2, STR_PROLOGUE_RADIO_NAG_3 };
			radio(nagIds[stage]);
		}
	}
	checkBranchB(bg);
	// onPlayerTurnStart is dispatched from SavedBattleGame::endTurn -- the same
	// context vanilla calls finishBattle from (turn-limit / tally paths), so an
	// armed ending can execute here directly.
	resolvePendingEnding(bg);
}

void CalypsoPrologueScene::onEnemyTurnStart(BattlescapeGame *bg)
{
	// Reset the per-Choir-turn step machine (D1: onEnemyTurnIdle pumps it).
	(void)bg;
	if (_inert) return;
	_gauntletStep = 0;
	_currentVictimId = -1;
	_shotsThisTurn = 0;
	_diverMissFired = false;
}

bool CalypsoPrologueScene::onEnemyTurnIdle(BattlescapeGame *bg)
{
	if (_inert || !bg) return false;
	// An armed ending outranks the step machine; it tears the battle down.
	if (resolvePendingEnding(bg)) return false;
	switch (_phase)
	{
		case Ph::MoveToOffice: return stepMoveToOffice(bg);
		case Ph::Ambushed:     return stepAmbushed(bg);
		case Ph::Gauntlet:     return stepGauntlet(bg);
		default:               return false; // Landing / Ended -- nothing to pump
	}
}

// --------------------------------------------------------------------------- //
// step machine
// --------------------------------------------------------------------------- //

bool CalypsoPrologueScene::stepMoveToOffice(BattlescapeGame *bg)
{
	SavedBattleGame *save = bg->getSave();
	BattleUnit *assessor = findUnit(save, _assessorId);
	if (!assessor || assessor->isOut())
	{
		// Shouldn't happen this early -- fail safe into the transition instead
		// of stalling the script forever.
		_phase = Ph::Ambushed;
		return true;
	}

	int assessorDist = Calypso::chebyshevToRect(assessor->getPosition().x, assessor->getPosition().y, EXIT_AREA);
	std::vector<int> otherDists;
	for (int id : allPlayerIds())
	{
		if (id == _assessorId) continue;
		BattleUnit *u = findUnit(save, id);
		if (!u || u->isOut()) continue;
		otherDists.push_back(Calypso::chebyshevToRect(u->getPosition().x, u->getPosition().y, EXIT_AREA));
	}

	if (!Calypso::ambushShouldFire(assessorDist, otherDists, TRIGGER_DIST, save->getTurn(), FALLBACK_TURN))
		return false; // nothing to do this Choir turn -- end it

	radio(STR_PROLOGUE_RADIO_AMBUSH);
	_phase = Ph::Ambushed;
	_shotsThisTurn = 0;
	return stepAmbushed(bg); // fire the first shot in the same call
}

bool CalypsoPrologueScene::stepAmbushed(BattlescapeGame *bg)
{
	SavedBattleGame *save = bg->getSave();
	BattleUnit *assessor = findUnit(save, _assessorId);
	if (!assessor || assessor->isOut())
	{
		// The Assessor is down -- evac order, Nikos handoff, gauntlet begins.
		radio(STR_PROLOGUE_RADIO_OBJECTIVE);
		if (BattleUnit *nikos = findUnit(save, _nikosId))
			CalypsoDirector::get().handoffToPlayer(bg, nikos);
		_phase = Ph::Gauntlet;
		_gauntletStep = 0;
		_currentVictimId = -1;
		_shotsThisTurn = 0;
		return stepGauntlet(bg); // continue into the gauntlet in the same call
	}

	BattleUnit *marksman = findUnit(save, _marksmanId);
	if (!marksman)
	{
		Log(LOG_ERROR) << "[prologue] marksman missing mid-ambush -- scene going inert";
		_inert = true;
		return false;
	}

	_shotsThisTurn++;
	if (_shotsThisTurn <= SHOT_CAP_PER_TURN)
		CalypsoDirector::get().directedShot(bg, marksman, assessor, false);
	else
		forceKill(bg, assessor, marksman);
	return true;
}

int CalypsoPrologueScene::pickGauntletTarget(SavedBattleGame *save) const
{
	std::vector<Calypso::UnitPos> candidates;

	if (!_firstDeathDone)
	{
		// D2: the first post-Assessor death is a forced pick -- the Leader
		// (real shot -> name line on death) or one of the two divers (scripted
		// miss -> line -> death), decided by the one-time _leaderDiesFirst roll.
		if (_leaderDiesFirst)
		{
			BattleUnit *leader = findUnit(save, _leaderId);
			if (leader && !leader->isOut()) return _leaderId;
		}
		else
		{
			for (int id : _diverIds)
			{
				BattleUnit *u = findUnit(save, id);
				if (u && !u->isOut()) candidates.push_back({ id, u->getPosition().x, u->getPosition().y });
			}
			if (!candidates.empty())
				return Calypso::pickGauntletVictim(candidates, EXIT_AREA, _nikosId);
		}
		// Forced target already gone (edge case, e.g. reaction fire) -- fall
		// through to the normal farthest-first pick below.
	}

	candidates.clear();
	for (int id : allPlayerIds())
	{
		BattleUnit *u = findUnit(save, id);
		if (u && !u->isOut()) candidates.push_back({ id, u->getPosition().x, u->getPosition().y });
	}
	return Calypso::pickGauntletVictim(candidates, EXIT_AREA, _nikosId);
}

bool CalypsoPrologueScene::stepGauntlet(BattlescapeGame *bg)
{
	SavedBattleGame *save = bg->getSave();

	if (_gauntletStep == 0)
	{
		steerActiveHerder(bg);
		_gauntletStep = 1;
		return true;
	}

	if (_gauntletStep == 1)
	{
		if (_currentVictimId < 0)
		{
			_currentVictimId = pickGauntletTarget(save);
			_diverMissFired = false;
			if (_currentVictimId < 0)
			{
				_gauntletStep = 2; // nobody left to shoot (all aboard or dead)
				return true;
			}
		}

		BattleUnit *victim = findUnit(save, _currentVictimId);
		if (!victim || victim->isOut())
		{
			_currentVictimId = -1;
			_gauntletStep = 2;
			return true;
		}

		BattleUnit *marksman = findUnit(save, _marksmanId);
		if (!marksman)
		{
			Log(LOG_ERROR) << "[prologue] marksman missing mid-gauntlet -- scene going inert";
			_inert = true;
			return false;
		}

		bool diverMissBeat = !_firstDeathDone && !_leaderDiesFirst && !_diverMissFired
			&& std::find(_diverIds.begin(), _diverIds.end(), _currentVictimId) != _diverIds.end();
		if (diverMissBeat)
		{
			CalypsoDirector::get().directedShot(bg, marksman, victim, true);
			radio(STR_PROLOGUE_DIVER_LINE);
			_diverMissFired = true;
			return true;
		}

		_shotsThisTurn++;
		if (_shotsThisTurn <= SHOT_CAP_PER_TURN)
			CalypsoDirector::get().directedShot(bg, marksman, victim, false);
		else
			forceKill(bg, victim, marksman);
		return true;
	}

	if (_gauntletStep == 2)
	{
		steerNikos(bg);
		_gauntletStep = 3;
		return true;
	}

	return false; // step machine idle -- end the Choir turn
}

void CalypsoPrologueScene::steerActiveHerder(BattlescapeGame *bg)
{
	SavedBattleGame *save = bg->getSave();

	BattleUnit *active = findUnit(save, activeHerderId());
	if (!active || active->isOut())
	{
		// Current herder is dead (or none picked yet) -- release the next
		// penned instance, if any survives.
		_activeHerderIdx = -1;
		for (size_t i = 0; i < _herderIds.size(); ++i)
		{
			BattleUnit *h = findUnit(save, _herderIds[i]);
			if (h && !h->isOut()) { _activeHerderIdx = (int)i; break; }
		}
		active = findUnit(save, activeHerderId());
	}
	if (!active) return; // both herders dead -- no visible pursuer, kills continue

	Position target = Calypso::chebyshevToRect(active->getPosition().x, active->getPosition().y, EXIT_AREA) <= 1
		? EXIT_AREA_CENTER : HERDER_WAYPOINT_MID;
	CalypsoDirector::get().steerUnit(bg, active, target);
	checkBranchB(bg);
}

void CalypsoPrologueScene::steerNikos(BattlescapeGame *bg)
{
	SavedBattleGame *save = bg->getSave();
	BattleUnit *nikos = findUnit(save, _nikosId);
	if (!nikos || nikos->isOut()) return;
	// The map guarantees >=5 turns from his SE start (41.1a step 5) -- he never
	// reaches the boat in time, but the scene walks him regardless of who
	// "controls" him (handoffToPlayer only swaps camera/selection ownership).
	CalypsoDirector::get().steerUnit(bg, nikos, EXIT_AREA_CENTER);
}

// --------------------------------------------------------------------------- //
// endings
// --------------------------------------------------------------------------- //

void CalypsoPrologueScene::checkBranchB(BattlescapeGame *bg)
{
	if (_phase != Ph::Gauntlet || _endingTriggered) return;
	SavedBattleGame *save = bg->getSave();

	bool anyoneAboard = false;
	bool anyDiverAliveOutside = false;
	for (int id : allPlayerIds())
	{
		BattleUnit *u = findUnit(save, id);
		if (!u || u->isOut()) continue;
		if (u->isInExitArea(START_POINT)) anyoneAboard = true;
		else anyDiverAliveOutside = true;
	}

	BattleUnit *herder = findUnit(save, activeHerderId());
	bool herderAtBoat = herder && !herder->isOut()
		&& Calypso::inRect(herder->getPosition().x, herder->getPosition().y, EXIT_AREA);

	// ARM only -- execution is deferred to resolvePendingEnding() on a clean
	// stack. This function is reachable from inside checkForCasualties (via
	// onUnitDied); killing more units or calling finishBattle from there would
	// re-enter the casualty loop mid-iteration (no vanilla precedent -- vanilla
	// detects death-driven battle ends later, at endTurn tally time).
	if (herderAtBoat && anyoneAboard)
	{
		// Branch Б: the Choir boards the Nereid. Not a finishing blow -- a
		// taking. No survivors.
		_endingTriggered = true;
		_pendingOutcome = OutcomeAllTaken;
		_pendingTaking = true;
		return;
	}

	if (!anyoneAboard && !anyDiverAliveOutside)
	{
		// Everyone else is gone and nobody made it aboard -- Nikos never
		// survives this either (design doc §8 #1: Branch В is closed).
		_endingTriggered = true;
		_pendingOutcome = OutcomeAllTaken;
		_pendingTaking = false;
	}
}

bool CalypsoPrologueScene::resolvePendingEnding(BattlescapeGame *bg)
{
	if (_pendingOutcome < 0 || !bg) return false;
	int outcome = _pendingOutcome;
	_pendingOutcome = -1;
	// _endingTriggered stays set -- onUnitDied's early-return keeps the nested
	// deaths below from re-arming or re-scripting anything.
	if (_pendingTaking)
	{
		radio(STR_PROLOGUE_RADIO_SILENCE);
		killEveryoneAboard(bg);
	}
	_pendingTaking = false;
	killNikosIfAlive(bg);
	CalypsoDirector::get().endScene(bg, outcome);
	return true;
}

void CalypsoPrologueScene::killNikosIfAlive(BattlescapeGame *bg)
{
	SavedBattleGame *save = bg->getSave();
	BattleUnit *nikos = findUnit(save, _nikosId);
	if (nikos && !nikos->isOut()) forceKill(bg, nikos, nullptr);
}

void CalypsoPrologueScene::killEveryoneAboard(BattlescapeGame *bg)
{
	SavedBattleGame *save = bg->getSave();
	for (int id : allPlayerIds())
	{
		BattleUnit *u = findUnit(save, id);
		if (u && !u->isOut() && u->isInExitArea(START_POINT))
			forceKill(bg, u, nullptr); // the Taking -- no attributed weapon
	}
}

void CalypsoPrologueScene::onUnitDied(BattlescapeGame *bg, BattleUnit *victim, BattleUnit *killer)
{
	(void)killer;
	if (_inert || !victim || !bg) return;
	int id = victim->getId();

	if (_endingTriggered)
	{
		// Ending armed/executing: the deaths below are the taking itself (or
		// post-cast-off cleanup) -- no beats, no re-arming, bookkeeping only.
		if (id == _currentVictimId) _currentVictimId = -1;
		return;
	}

	if (id == _leaderId && !_firstDeathDone && _phase == Ph::Gauntlet)
	{
		// D2: guaranteed payoff regardless of a 1-shot or 2-shot kill.
		radio(STR_PROLOGUE_LEADER_NAME);
		_firstDeathDone = true;
	}
	else if (id == _currentVictimId)
	{
		_firstDeathDone = true; // later gauntlet deaths are silent
	}

	if (id == _currentVictimId) _currentVictimId = -1;

	checkBranchB(bg);
}

bool CalypsoPrologueScene::onAbortRequested(BattlescapeState *bs)
{
	if (_inert || !bs) return false;
	if (_phase != Ph::Ambushed && _phase != Ph::Gauntlet) return false;

	BattlescapeGame *bg = bs->getBattleGame();
	if (!bg) return false;
	SavedBattleGame *save = bg->getSave();
	if (!save) return false;

	bool anyoneAboard = false;
	for (int id : allPlayerIds())
	{
		BattleUnit *u = findUnit(save, id);
		if (u && !u->isOut() && u->isInExitArea(START_POINT)) { anyoneAboard = true; break; }
	}
	// Nobody aboard -- let vanilla abort-mission rules handle it (the scene
	// does not consume the confirmation; whether the vanilla flow itself
	// blocks an empty-boat abort is verified at browser QA, not statically).
	if (!anyoneAboard) return false;

	// Set the guard BEFORE the kill: Nikos's death re-enters onUnitDied, and an
	// unguarded checkBranchB there could arm Branch Б mid-cast-off (e.g. the
	// herder reaching the boat on the very turn the player casts off).
	_endingTriggered = true;

	if (BattleUnit *nikos = findUnit(save, _nikosId))
	{
		if (!nikos->isOut())
		{
			radio(STR_PROLOGUE_NIKOS_LINE);
			forceKill(bg, nikos, nullptr); // off-screen, unattributed
		}
	}

	// Synchronous endScene is safe here: this is AbortMissionState::btnOkClick,
	// the exact call site vanilla itself invokes finishBattle from.
	CalypsoDirector::get().endScene(bg, OutcomeCastOff);
	return true;
}

bool CalypsoPrologueScene::abortStrings(std::string *title, std::string *ok, std::string *cancel)
{
	if (_inert) return false;
	if (_phase != Ph::Ambushed && _phase != Ph::Gauntlet) return false;
	if (title)  *title  = "STR_PROLOGUE_CASTOFF_TITLE";
	if (ok)     *ok     = "STR_PROLOGUE_CASTOFF_OK";
	if (cancel) *cancel = "STR_PROLOGUE_CASTOFF_CANCEL";
	return true;
}

State *CalypsoPrologueScene::makeEndState()
{
	// Commit 4 adds CalypsoPrologueEndState ("Six months later" interstitial).
	// Null here just means "no custom end state yet" -- interceptFinishBattle
	// still consumes the outcome and suppresses the vanilla debrief.
	return nullptr;
}

// --------------------------------------------------------------------------- //
// primitives
// --------------------------------------------------------------------------- //

void CalypsoPrologueScene::forceKill(BattlescapeGame *bg, BattleUnit *victim, BattleUnit *attacker)
{
	if (!bg || !victim) return;
	SavedBattleGame *save = bg->getSave();
	if (!save) return;
	Mod *mod = bg->getMod();
	const RuleDamageType *dt = mod ? mod->getDamageType(DT_AP) : nullptr;
	if (!dt)
	{
		Log(LOG_ERROR) << "[prologue] forceKill: no DT_AP damage type in ruleset";
		return;
	}

	BattleActionAttack attack;
	attack.type = BA_SNAPSHOT;
	attack.attacker = attacker; // may be null -- unattributed/off-screen death

	// damage() only reduces HP/wound state; checkForCasualties() is what
	// finalizes the kill (morale, attribution, corpse via UnitDieBState) --
	// see BattlescapeGame::checkForCasualties and ExplosionBState's real
	// damage()-then-checkForCasualties call site.
	victim->damage(Position(0, 0, 0), 999, dt, save, attack);
	bg->checkForCasualties(dt, attack, false, false);
}

void CalypsoPrologueScene::radio(const std::string &stringId) const
{
	CalypsoDirector::get().radioLine(getCurrentGame(), stringId);
}

// --------------------------------------------------------------------------- //
// persistence
// --------------------------------------------------------------------------- //

void CalypsoPrologueScene::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("phase", (int)_phase);
	writer.write("inert", _inert);
	writer.write("endingTriggered", _endingTriggered);
	writer.write("pendingOutcome", _pendingOutcome);
	writer.write("pendingTaking", _pendingTaking);
	writer.write("leaderId", _leaderId);
	writer.write("diverIds", _diverIds);
	writer.write("assessorId", _assessorId);
	writer.write("nikosId", _nikosId);
	writer.write("marksmanId", _marksmanId);
	writer.write("herderIds", _herderIds);
	writer.write("leaderDiesFirst", _leaderDiesFirst);
	writer.write("firstDeathDone", _firstDeathDone);
	writer.write("diverMissFired", _diverMissFired);
	writer.write("gauntletStep", _gauntletStep);
	writer.write("currentVictimId", _currentVictimId);
	writer.write("shotsThisTurn", _shotsThisTurn);
	writer.write("activeHerderIdx", _activeHerderIdx);
	writer.write("lastNagStage", _lastNagStage);
}

void CalypsoPrologueScene::load(const YAML::YamlNodeReader &reader)
{
	_phase = (Ph)reader["phase"].readVal<int>((int)Ph::Landing);
	_inert = reader["inert"].readVal<bool>(false);
	_endingTriggered = reader["endingTriggered"].readVal<bool>(false);
	_pendingOutcome = reader["pendingOutcome"].readVal<int>(-1);
	_pendingTaking = reader["pendingTaking"].readVal<bool>(false);
	_leaderId = reader["leaderId"].readVal<int>(-1);
	_diverIds = reader["diverIds"].readVal<std::vector<int>>(std::vector<int>{});
	_assessorId = reader["assessorId"].readVal<int>(-1);
	_nikosId = reader["nikosId"].readVal<int>(-1);
	_marksmanId = reader["marksmanId"].readVal<int>(-1);
	_herderIds = reader["herderIds"].readVal<std::vector<int>>(std::vector<int>{});
	_leaderDiesFirst = reader["leaderDiesFirst"].readVal<bool>(false);
	_firstDeathDone = reader["firstDeathDone"].readVal<bool>(false);
	_diverMissFired = reader["diverMissFired"].readVal<bool>(false);
	_gauntletStep = reader["gauntletStep"].readVal<int>(0);
	_currentVictimId = reader["currentVictimId"].readVal<int>(-1);
	_shotsThisTurn = reader["shotsThisTurn"].readVal<int>(0);
	_activeHerderIdx = reader["activeHerderIdx"].readVal<int>(-1);
	_lastNagStage = reader["lastNagStage"].readVal<int>(-1);
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
