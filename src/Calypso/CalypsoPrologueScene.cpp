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
#include "CalypsoPrologueCampaign.h"
#include "CalypsoPrologueEndState.h"
#include "CalypsoTutorial.h"

#include "../Battlescape/BattlescapeGame.h"
#include "../Battlescape/BattlescapeState.h"
#include "../Battlescape/Map.h"
#include "../Battlescape/Camera.h"
#include "../Battlescape/TileEngine.h" // calculateFOV() after the marksman teleport
#include "../Battlescape/Pathfinding.h" // review round 2 finding 2: real path-cost gate
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/Soldier.h"  // getGeoscapeSoldier() survivor snapshot
#include "../Savegame/Tile.h"     // review round 2 finding 1: START_POINT slot scan
#include "../Savegame/BattleItem.h" // amendment #10: corpse-item collection
#include "../Mod/Unit.h"          // UnitFaction, SpecialTileType, UnitStats
#include "../Mod/MapData.h"       // SpecialTileType::START_POINT, MovementType::MT_WALK
#include "../Mod/RuleDamageType.h"
#include "../Mod/Mod.h"
#include "../Engine/RNG.h"
#include "../Engine/Game.h"       // getCurrentGame()
#include "../Engine/Logger.h"
#include "../Engine/Yaml.h"

namespace OpenXcom
{

// --------------------------------------------------------------------------- //
// tunables -- calibrated against the real assembled map (QA round 1, browser
// measurement via ?scenePreview + a quicksave unit-position dump; see
// docs/phases/phase-41-tutorial-mission.md §41.1a step 5 for the grid). Every
// use site below used to be marked TUNE(41.1a step 5) -- that pass is done.
// --------------------------------------------------------------------------- //

// Craft (Nereid) exit/deployment zone, in tile coordinates. The mapScript
// places the TRITON craft block at grid [4,0] w1 h2 (10x20 tiles = x40-49,
// y0-19; calypso-prologue.rul mapScripts comment). The craft hull only fills
// part of that block -- measured via the scene-preview coordinate readout by
// hovering the hull's four extremes (41,9) (47,6) (43,11) (45,1) and cross-
// checked against a quicksave dump of the 3 SOLDIER spawn tiles, which
// clustered tightly at (45,7) (44,7) (45,6) -- comfortably inside this rect.
static const Calypso::Rect EXIT_AREA{ 40, 0, 48, 12 };
static const Position EXIT_AREA_CENTER(44, 6, 0);

// Distance (Chebyshev tiles) the Assessor + one other unit must clear from the
// Nereid before the ambush can trigger. Office cluster occupies grid [0,0]
// w2 h2 (x0-19, y0-19); its nearest edge (x=19) to EXIT_AREA's nearest edge
// (x=40) is 21 tiles -- the total craft-to-office march. "Roughly two full TU
// sprints" (41.1a) would overshoot that at ~12-15 tiles/turn, so the trigger
// is set to ~2/3 of the total distance instead: far enough that turn-1 (spawn
// distance 0) never fires, close enough to the office that the squad is
// genuinely "en route" (not arrived) when the ambush hits, matching the
// turns-3-5 window design (FIRST_NAG_TURN=3 below already fires on schedule --
// unaffected, it's turn-based not distance-based).
static const int TRIGGER_DIST = 14;
// Unconditional ambush turn if the distance trigger never fires (sabotage fallback).
static const int FALLBACK_TURN = 8;
// First player turn an escalating nag radio line can fire (see escalationStage()).
static const int FIRST_NAG_TURN = 3;
// Repeat-fire shots at one victim before the direct-damage fallback kicks in.
static const int SHOT_CAP_PER_TURN = 4;
// Review round 3 (P1): minimum number of PLAYER turns Nikos's scripted post
// must keep him from the Nereid (he has no other mover) -- see
// checkNikosPathCost.
static const int NIKOS_MIN_TURNS = 5;

// Herder waypoints: pen -> squad midpoint -> the Nereid itself. Midpoint of
// the craft-to-office march (EXIT_AREA_CENTER (44,6) <-> office center
// roughly (10,10)) along the row0/row1 filler blocks (grid cols 2-3) the
// squad actually crosses -- NOT the row2 (y20-29) "open gauntlet path" cells,
// which sit south of both the craft and the office and are off the direct
// route (mapScript comment describing row2 as the gauntlet path predates the
// final craft/office grid placement; both landmarks ended up in the top band).
static const Position HERDER_WAYPOINT_MID(27, 8, 0);

// Marksman perch (QA round 1 bug 7): every populated RMP node in every PORT
// block is rank 0 (ground-level, civilian/scout) -- there is no elevated or
// alien-specific spawn node anywhere on this terrain, so the marksman's
// natural spawn is just wherever the generic node picker lands (observed at
// (7,4,0), inside the office block -- not a sniper nest by any definition).
// Per the QA-approved fallback (docs/phases/phase-41-tutorial-mission.md,
// "prefer NOT teleporting" revisited), CalypsoPrologueScene::onBattleStart
// now teleports the marksman onto a fixed ground tile along the squad's march
// route instead: open yard tiles in the row0/row1 filler blocks, close enough
// to the path for directedShot()'s real projectile pipeline to have a clear
// line once the ambush fires near HERDER_WAYPOINT_MID. PERCH_B is the
// fallback if PERCH_A is occupied/blocked (SavedBattleGame::setUnitPosition
// returns false and the scene tries the next candidate).
static const Position MARKSMAN_PERCH_A(30, 5, 0);
static const Position MARKSMAN_PERCH_B(20, 12, 0);

// Named-actor scripted tiles (review round 1, P1). Grid recap
// (calypso-prologue.rul mapScripts): office x0-19/y0-19, craft hull inside
// x40-48/y0-12, PORT18 SE guard post block at grid cell [3,3] w2 h2 == tiles
// x30-49/y30-49 (NOT the y20-29 open band -- that is filler PORT10-16
// blocks, one grid row further north).
//
// Review round 2 (P1, finding 2): the previous NIKOS_POST candidates
// (46,26)/(44,26)/(47,23) sit in that y20-29 filler band, OUTSIDE the actual
// SE guard-post block -- only 11-14 tiles from the Nereid, not the designed
// >= 5 turns. Fixed by hand from a direct parse of PORT18.MAP (the tileset's
// own floor/object grid -- the same "floor present, no O_OBJECT" predicate
// BattlescapeGenerator::canPlaceXCOMUnit uses, see the tool this review used:
// scripts are throwaway, the parsed coordinates are the deliverable): local
// (10,10)/(14,16)/(12,12) on PORT18 are plain open floor tiles (MCD floor
// id 36/37/37, no object), offset by the block's grid placement (+30,+30) to
// map coordinates. All three land well inside x30-49/y30-49. The scene
// preview (?scenePreview=STR_CALYPSO_PROLOGUE) does not exercise this code
// path (CalypsoDirector suppresses scene construction entirely in preview
// mode, so placeNamedActors never runs there) -- MAP-parsing was the
// reviewer-sanctioned fallback for this reason. A real Pathfinding-cost gate
// (checkNikosPathCost, called right after placement) is the actual
// safety net, not raw tile distance -- see that function for why.
static const Position NIKOS_POST[3]    = { Position(40, 40, 0), Position(44, 46, 0), Position(42, 42, 0) }; // SE guard post (PORT18 block interior)
// Review round 2 (P1, finding 1): fixed candidates removed -- the Assessor is
// now placed by placeAssessorOnFreeCraftSlot(), which scans the Nereid's real
// START_POINT deployment tiles for a free one instead of guessing fixed
// coordinates that could land on an already-occupied crew slot.
static const Position HERDER_PEN[2][3] = {
	{ Position(2, 26, 0), Position(4, 26, 0), Position(2, 24, 0) },  // pen, far SW -- ~42 tiles from the squad, beyond sight range
	{ Position(6, 26, 0), Position(8, 26, 0), Position(6, 24, 0) },
};

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
// Review round 2 (P1, finding 3): HYBRID design contract -- prologue-specific
// hint beats (movement/camera/TU), delivered through the same radio-toast
// primitive as the narrative lines above. See the onBattleStart comment.
static const char *STR_PROLOGUE_HINT_MOVE       = "STR_PROLOGUE_HINT_MOVE";
static const char *STR_PROLOGUE_HINT_CAMERA     = "STR_PROLOGUE_HINT_CAMERA";
static const char *STR_PROLOGUE_HINT_TU         = "STR_PROLOGUE_HINT_TU";

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
	// Deployment invariant: STR_CALYPSO_PROLOGUE must not define turnLimit (or
	// a chronoTrigger that depends on it). BattlescapeGame's timer expiry calls
	// finishBattle() directly, bypassing onAbortRequested(); ConsumeAbort in
	// onUnexpectedFinish() would then consume that external finish on every
	// subsequent turn and soft-lock the scene. Validate this in release builds,
	// too: a malformed deployment goes inert and resolves through the existing
	// controlled all-taken fallback on the next safe director callback.
	SavedBattleGame *save = bg ? bg->getSave() : nullptr;
	if (!save || !Calypso::prologueTurnLimitIsSafe(save->getTurnLimit()))
	{
		Log(LOG_ERROR) << "[prologue] invalid battle start: "
			<< (!save ? "missing battlescape save" : "turnLimit must be disabled")
			<< (!save ? "" : " (configured value " + std::to_string(save->getTurnLimit()) + ")")
			<< "; arming all-taken fallback";
		_inert = true;
		_endingTriggered = true;
		_pendingOutcome = OutcomeAllTaken;
		return;
	}
	if (!resolveActors(bg))
	{
		_inert = true;
		_endingTriggered = true;
		_pendingOutcome = OutcomeAllTaken;
		return;
	}
	placeMarksman(bg);
	if (BattleUnit *marksman = findUnit(bg->getSave(), _marksmanId))
		marksman->setScriptedConcealed(true);
	if (bg->getSave()->getTileEngine())
		bg->getSave()->getTileEngine()->recalculateFOV();
	// Review round 2 (P1, finding 1): the Assessor MUST land on a real free
	// craft deployment slot -- a mis-placed Assessor breaks the ambush-
	// trigger geometry (TRIGGER_DIST is measured from EXIT_AREA/the Nereid).
	// No free slot is a hard failure -- go inert rather than continue with
	// broken staging (same contract as resolveActors()).
	if (!placeAssessorOnFreeCraftSlot(bg))
	{
		_inert = true;
		_endingTriggered = true;
		_pendingOutcome = OutcomeAllTaken;
		return;
	}
	placeNamedActors(bg);
	checkNikosPathCost(bg); // review round 3 (P1) -- diagnostic, after Nikos is placed
	// Review round 1 (P1): the Assessor must be escortable -- as a neutral
	// civilian the player physically cannot "walk him to the office" and the
	// distance trigger would depend on civilian-AI wandering. Hand him to the
	// player from turn 0 (same convertToFaction primitive as the Nikos
	// handoff; _originalFaction stays neutral for the debrief tally). He is
	// NOT in allPlayerIds()/_diverIds (resolved above, by faction, before
	// this call), so the gauntlet picker and Branch B never count him.
	if (BattleUnit *assessor = findUnit(bg->getSave(), _assessorId))
		CalypsoDirector::get().handoffToPlayer(bg, assessor);
	// D2: rolled once, drives which pattern the first post-Assessor death uses.
	_leaderDiesFirst = RNG::percent(50);
	_phase = Ph::MoveToOffice;

	// Review round 2 (P1, finding 3): the generic Phase 37/39 battlescape
	// tutorial is correctly suppressed for the whole prologue battle
	// (CalypsoPrologueCampaign.cpp, launchScriptedBattle) -- its content
	// promises the wrong mission-end condition and teaches shooting/kneeling
	// this scene never needs. But the phase plan's HYBRID design (Goal
	// section) still requires the prologue to teach movement, camera, and TU
	// before the ambush -- "a good prologue but not a tutorial" was the
	// review-round-2 finding. These three beats are prologue-specific
	// content delivered through the SAME radio-toast primitive already used
	// for narrative lines (radio() / CalypsoDirector::radioLine) -- no new
	// UI, no dependency on CalypsoTutorial's disabled singleton (so nothing
	// here can accidentally re-enable it, unlike reusing CalypsoTutorialState
	// directly would -- its "disable" button flips that shared flag).
	// STAGED, not burst (review polish): a four-toast LIFO pile-up at battle
	// start (~2s each, stacked on the landing line) reads as a splash screen,
	// not teaching. One beat per player turn instead, each at the moment it
	// becomes relevant: MOVE here on turn 1 (the first thing the player must
	// do), CAMERA on turn 2, TU on turn 3 -- see onPlayerTurnStart; the later
	// beats are gated on Ph::MoveToOffice so a firefight never gets a
	// tutorial toast. Pushed in REVERSE of on-screen order (LIFO stack): the
	// landing line shows first, then the movement hint.
	radio(STR_PROLOGUE_HINT_MOVE, CalypsoRadioLineKind::Instruction);
	radio(STR_PROLOGUE_RADIO_LANDING);
}

// QA round 1 bug 7: the terrain has no elevated/alien-specific RMP nodes, so
// the marksman's generic-node spawn lands wherever (observed inside the
// office block). Teleport him onto a fixed perch along the march route
// instead. SavedBattleGame::setUnitPosition is the same relink primitive
// BattlescapeGenerator::addXCOMUnit's craft-inventory-tile repositioning and
// SavedBattleGame::resetUnitTiles use -- it validates the destination (tile
// exists, unoccupied, not a big-wall object, has floor) and on success calls
// BattleUnit::setTile() + setPosition(), which fully relinks both directions
// (old tile's getUnit() cleared, new tile's getUnit() set to this unit) --
// see BattleUnit::setTile (src/Savegame/BattleUnit.cpp). PERCH_B is tried if
// PERCH_A is blocked; if both fail the marksman just stays at his spawn node
// (inert-safe, not a hard failure -- directedShot only needs *a* position).
void CalypsoPrologueScene::placeMarksman(BattlescapeGame *bg)
{
	SavedBattleGame *save = bg->getSave();
	BattleUnit *marksman = findUnit(save, _marksmanId);
	if (!marksman) return;

	if (!save->setUnitPosition(marksman, MARKSMAN_PERCH_A)
		&& !save->setUnitPosition(marksman, MARKSMAN_PERCH_B))
	{
		Log(LOG_WARNING) << "[prologue] marksman perch teleport failed (both candidates blocked) -- "
			<< "leaving spawn position " << marksman->getPosition();
		return;
	}
	// Mirrors the "newly-placed unit" FOV-population pattern used when aliens
	// are spawned mid-battle (BattlescapeGame.cpp:377) -- the generator's own
	// initial placement doesn't need this (a full recalc runs once at battle
	// start), but a scene-side reposition after that point does.
	save->getTileEngine()->calculateFOV(marksman);
}

// Review round 1 (P1): teleport the remaining named actors onto their
// scripted tiles (see the NIKOS_POST/HERDER_PEN comment above). Failure is a
// loud warning, not inert: a mis-placed actor degrades the staging but every
// script beat still works from any position. (The Assessor is placed
// separately by placeAssessorOnFreeCraftSlot -- review round 2 finding 1 --
// because his placement failure IS a hard inert condition.)
void CalypsoPrologueScene::placeNamedActors(BattlescapeGame *bg)
{
	SavedBattleGame *save = bg->getSave();

	auto placeAt = [save](BattleUnit *u, const Position *candidates, int n, const char *who)
	{
		if (!u) return;
		for (int i = 0; i < n; ++i)
		{
			if (save->setUnitPosition(u, candidates[i]))
			{
				save->getTileEngine()->calculateFOV(u);
				return;
			}
		}
		Log(LOG_WARNING) << "[prologue] " << who << " scripted-tile teleport failed (all candidates blocked) -- "
			<< "leaving spawn position " << u->getPosition();
	};

	placeAt(findUnit(save, _nikosId), NIKOS_POST, 3, "nikos");
	for (size_t h = 0; h < _herderIds.size() && h < 2; ++h)
		placeAt(findUnit(save, _herderIds[h]), HERDER_PEN[h], 3, "herder");
}

// Review round 2 (P1, finding 1): the fixed ASSESSOR_POST candidates could
// land on an already-occupied Nereid crew slot (the 3 regular soldiers spawn
// there first) and left the Assessor stranded at his generic RMP spawn
// (observed in browser QA: "leaving spawn position (2,33,0)"). The Nereid's
// craft ruleset (calypso-prologue.rul `deployment:`) defines 4 START_POINT
// tiles for 3 regular crew, so exactly one is guaranteed free -- scan for it
// using the SAME predicate BattlescapeGenerator::canPlaceXCOMUnit applies when
// the generator itself places real crew (BattlescapeGenerator.cpp,
// canPlaceXCOMUnit): a tile whose floor carries the START_POINT special type,
// has no O_OBJECT (no big-wall/clutter blocking it), and has a walkable floor
// TU cost. setUnitPosition() re-validates occupancy/walkability itself, so
// this is safe to call on every START_POINT tile in map order and just take
// the first one that succeeds.
bool CalypsoPrologueScene::placeAssessorOnFreeCraftSlot(BattlescapeGame *bg)
{
	SavedBattleGame *save = bg->getSave();
	BattleUnit *assessor = findUnit(save, _assessorId);
	if (!assessor)
	{
		Log(LOG_ERROR) << "[prologue] assessor: unit missing before slot search -- scene going inert";
		return false;
	}

	for (int i = 0; i < save->getMapSizeXYZ(); ++i)
	{
		Tile *t = save->getTile(i);
		if (!t || t->getFloorSpecialTileType() != START_POINT) continue;
		if (t->getMapData(O_OBJECT)) continue;
		MapData *floor = t->getMapData(O_FLOOR);
		if (!floor || floor->getTUCost(MT_WALK) == Pathfinding::INVALID_MOVE_COST) continue;

		if (save->setUnitPosition(assessor, t->getPosition()))
		{
			save->getTileEngine()->calculateFOV(assessor);
			return true;
		}
	}

	Log(LOG_ERROR) << "[prologue] assessor: no free Nereid deployment slot found (all START_POINT "
		<< "tiles occupied/blocked) -- scene going inert";
	return false;
}

// Review round 3 (P1): the scene does NOT walk Nikos after the handoff --
// the player is his only mover. The earlier design (director steerNikos()
// each Choir turn + a pinned-turns delay) double-dipped: the player moved
// him on player turns AND the director refilled his TU and moved him again
// on Choir turns, so the pin arithmetic undercounted and a measured 146-TU
// path collapsed to ~3 game rounds; worse, a Nikos who DID reach the boat
// was then inexplicably force-killed by the ending. With a single mover the
// guarantee is plain division: path cost / his per-turn TU (146 / 35 -> 5
// player turns on the calibrated map). This check verifies that ratio
// against NIKOS_MIN_TURNS at battle start and logs the verdict -- it is a
// map-calibration DIAGNOSTIC, not a runtime gate (nothing is pinned).
//
// It is still not a survival gate: Nikos is unconditionally force-killed by
// killNikosIfAlive()/onAbortRequested() whenever any ending resolves
// (design doc §8 #1: "Branch В is closed"). A player sprinting him
// boat-ward arrives no earlier than NIKOS_MIN_TURNS rounds -- by which time
// the gauntlet has resolved the retreat one way or the other.
void CalypsoPrologueScene::checkNikosPathCost(BattlescapeGame *bg)
{
	SavedBattleGame *save = bg->getSave();
	BattleUnit *nikos = findUnit(save, _nikosId);
	Pathfinding *pf = save ? save->getPathfinding() : nullptr;
	if (!nikos || !pf) return;

	// maxTUCost=100000: we want the TRUE path cost, not one capped at some
	// unit's remaining TU (the default cap is 1000, already generous, but an
	// unreachable/very long route must resolve as "safe", not as cost 0).
	pf->calculate(nikos, EXIT_AREA_CENTER, BAM_NORMAL, nullptr, 100000);
	bool reachable = !pf->getPath().empty();
	int cost = pf->getTotalTUCost();

	// Arrival turn = ceil(cost / tu); the guarantee is arrival >= turn
	// NIKOS_MIN_TURNS, i.e. cost must EXCEED (N-1) full turn budgets (the
	// calibrated map: 146 > 4*35=140 -> arrives on turn 5 -- satisfied).
	int tu = nikos->getBaseStats()->tu;
	int requiredCost = (NIKOS_MIN_TURNS - 1) * tu; // single mover: the player

	if (!reachable || cost > requiredCost)
	{
		Log(LOG_INFO) << "[prologue] nikos path-cost check: reachable=" << reachable
			<< " cost=" << cost << " required>" << requiredCost
			<< " -- >= " << NIKOS_MIN_TURNS << " player turns guaranteed";
	}
	else
	{
		// Map-calibration regression: move NIKOS_POST further from the boat.
		Log(LOG_WARNING) << "[prologue] nikos path-cost check: reachable=" << reachable
			<< " cost=" << cost << " required>" << requiredCost
			<< " -- Nikos can reach the boat in under " << NIKOS_MIN_TURNS
			<< " player turns; recalibrate NIKOS_POST (41.1a step 5)";
	}
}

void CalypsoPrologueScene::reconcileScriptedUnitState(BattlescapeGame *bg)
{
	if (!bg) return;
	SavedBattleGame *save = bg->getSave();
	if (!save) return;

	bool visibilityChanged = false;
	if (BattleUnit *marksman = findUnit(save, _marksmanId))
	{
		const bool shouldConceal = Calypso::shouldConcealPrologueMarksman(_marksmanRevealed);
		if (marksman->isScriptedConcealed() != shouldConceal)
		{
			marksman->setScriptedConcealed(shouldConceal);
			visibilityChanged = true;
		}
	}

	// The Assessor is playable from landing until the scripted ambush removes
	// him. Reassert the explicit persistent mode for old autosaves written when
	// the handoff still used temporary faction conversion.
	if ((_phase == Ph::MoveToOffice || _phase == Ph::Ambushed))
	{
		if (BattleUnit *assessor = findUnit(save, _assessorId))
			if (!assessor->isOut() && !assessor->hasScriptedPlayerControl())
				CalypsoDirector::get().handoffToPlayer(bg, assessor);
	}

	// Gauntlet and evacuation-only phases imply that the post-ambush handoff
	// completed. This also repairs old saves in which Nikos had already reverted
	// to neutral before the first actionable player turn.
	if ((_phase == Ph::Gauntlet || _evacOnly) && _nikosHandedOff)
	{
		if (BattleUnit *nikos = findUnit(save, _nikosId))
			if (!nikos->isOut() && !nikos->hasScriptedPlayerControl())
				CalypsoDirector::get().handoffToPlayer(bg, nikos);
	}

	if (visibilityChanged && save->getTileEngine())
		save->getTileEngine()->recalculateFOV();
}

void CalypsoPrologueScene::revealMarksman(BattlescapeGame *bg)
{
	if (_marksmanRevealed) return;
	_marksmanRevealed = true;
	if (!bg) return;
	if (BattleUnit *marksman = findUnit(bg->getSave(), _marksmanId))
		marksman->setScriptedConcealed(false);
}

void CalypsoPrologueScene::focusNikosOnPlayerTurn(BattlescapeGame *bg)
{
	if (!_nikosFocusPending || !bg) return;
	SavedBattleGame *save = bg->getSave();
	BattleUnit *nikos = findUnit(save, _nikosId);
	if (!nikos || nikos->isOut() || nikos->getFaction() != FACTION_PLAYER) return;

	nikos->setVisible(true);
	save->setSelectedUnit(nikos);
	if (Map *map = bg->getMap())
		if (Camera *cam = map->getCamera())
			cam->centerOnPosition(nikos->getPosition());
	if (BattlescapeState *state = save->getBattleState())
		state->updateSoldierInfo();
	_nikosFocusPending = false;
}

void CalypsoPrologueScene::onPlayerTurnStart(BattlescapeGame *bg)
{
	if (!bg) return;
	if (resolvePendingEnding(bg)) return;
	if (_inert) return;
	SavedBattleGame *save = bg->getSave();
	if (!save) return;
	reconcileScriptedUnitState(bg);
	focusNikosOnPlayerTurn(bg);

	// Panic-driven loss of control would fight the direction (41.3).
	CalypsoDirector::get().pinMorale(save, FACTION_PLAYER);

	// Amendment #10: the water collects last turn's fallen -- before the
	// autosave below, so a reload never resurrects a collected body.
	collectTakenBodies(bg);

	// D6: rolling anti-savescum autosave -- one slot, overwritten every player
	// turn. Safe to skip once an ending is armed/executing (Ph::Ended has no
	// further turns; a fresh write here would just get deleted moments later
	// by finishPrologue anyway, but skipping avoids a pointless disk write).
	if (!_endingTriggered)
	{
		CalypsoDirector::get().forceAutosave(getCurrentGame(), Calypso::PROLOGUE_AUTOSAVE_FILENAME);
	}

	if (_phase == Ph::MoveToOffice)
	{
		// Review round 2 (P1, finding 3), staged hint beats 2 and 3 -- see the
		// onBattleStart comment. Turn numbers are unique and this dispatch
		// fires once per player turn, so no one-shot flags are needed; the
		// Ph::MoveToOffice gate above keeps hints out of the post-ambush
		// firefight. On turn 3 the first nag line (below) may push after the
		// TU hint -- two short toasts on one turn, nag first (LIFO), is fine.
		if (save->getTurn() == 2)      radio(STR_PROLOGUE_HINT_CAMERA, CalypsoRadioLineKind::Instruction);
		else if (save->getTurn() == 3) radio(STR_PROLOGUE_HINT_TU, CalypsoRadioLineKind::Instruction);

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
	if (_inert) return;
	reconcileScriptedUnitState(bg);
	_gauntletStep = 0;
	_currentVictimId = -1;
	_shotsThisTurn = 0;
	_diverMissFired = false;
}

bool CalypsoPrologueScene::onEnemyTurnIdle(BattlescapeGame *bg)
{
	if (!bg) return false;
	// An armed ending outranks the step machine; it tears the battle down.
	if (resolvePendingEnding(bg)) return false;
	if (_inert || _evacOnly) return false;
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
		revealMarksman(bg);
		radio(STR_PROLOGUE_RADIO_OBJECTIVE);
		if (BattleUnit *nikos = findUnit(save, _nikosId))
		{
			CalypsoDirector::get().handoffToPlayer(bg, nikos);
			_nikosHandedOff = true;
			_nikosFocusPending = true;
		}
		_phase = Ph::Gauntlet;
		_gauntletStep = 0;
		_currentVictimId = -1;
		_shotsThisTurn = 0;
		return stepGauntlet(bg); // continue into the gauntlet in the same call
	}

	BattleUnit *marksman = findUnit(save, _marksmanId);
	if (!marksman || marksman->isOut())
	{
		Log(LOG_INFO) << "[prologue] marksman neutralized mid-ambush -- switching to extraction-only tail";
		enterEvacOnly(bg, true);
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
			if (leader && !leader->isOut()) candidates.push_back({ _leaderId,
				leader->getPosition().x, leader->getPosition().y,
				leader->isInExitArea(START_POINT) });
		}
		else
		{
			for (int id : _diverIds)
			{
				BattleUnit *u = findUnit(save, id);
				if (u && !u->isOut()) candidates.push_back({ id, u->getPosition().x, u->getPosition().y,
					u->isInExitArea(START_POINT) });
			}
		}
		if (!candidates.empty())
		{
			int forced = Calypso::pickGauntletVictim(candidates, EXIT_AREA, _nikosId);
			if (forced >= 0) return forced;
		}
		// Forced target already gone (edge case, e.g. reaction fire) -- fall
		// through to the normal farthest-first pick below.
	}

	candidates.clear();
	for (int id : allPlayerIds())
	{
		BattleUnit *u = findUnit(save, id);
		if (u && !u->isOut()) candidates.push_back({ id, u->getPosition().x, u->getPosition().y,
			u->isInExitArea(START_POINT) });
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
		// The walk queued by step 0 has completed before this pump. Re-check the
		// boarding ending against the herder's NEW position before selecting or
		// shooting another victim; otherwise Branch B gets one spurious kill.
		checkBranchB(bg);
		if (resolvePendingEnding(bg)) return false;

		// Amendment #9: consume a bought turn -- the Choir spends it releasing
		// the replacement herder (steerActiveHerder above already did that),
		// no victim is picked. Never consumed against the FIRST post-Assessor
		// loss: that beat (design doc s8 #2) is uncancellable, so the reprieve
		// is simply held until the first loss has happened.
		if (_currentVictimId < 0 && _herderReprieveTurns > 0 && _firstDeathDone)
		{
			--_herderReprieveTurns;
			_gauntletStep = 2;
			return true;
		}
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
		if (!marksman || marksman->isOut())
		{
			Log(LOG_INFO) << "[prologue] marksman neutralized mid-gauntlet -- switching to extraction-only tail";
			enterEvacOnly(bg, false);
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

	// (Review round 3, P1: there is deliberately no steerNikos step here --
	// after the handoff the PLAYER is Nikos's only mover. The director
	// walking him too doubled his effective speed and broke the >= 5-turn
	// guarantee; see checkNikosPathCost. Old saves with _gauntletStep == 2/3
	// fall through to idle harmlessly.)
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

	// Waypoint progression (review round 1, P1): promote the target to the
	// Nereid once the midpoint is reached or passed. The old test ("target
	// the boat only when already within 1 tile of the boat") could never
	// promote past the midpoint, so Branch B was unreachable. The pen is
	// west of the midpoint and the boat east of it, so "x beyond the
	// midpoint" is a monotonic progress test that also survives save/load.
	const Position herderPos = active->getPosition();
	const Calypso::Rect midRect{ HERDER_WAYPOINT_MID.x, HERDER_WAYPOINT_MID.y, HERDER_WAYPOINT_MID.x, HERDER_WAYPOINT_MID.y };
	bool pastMid = herderPos.x >= HERDER_WAYPOINT_MID.x
		|| Calypso::chebyshevToRect(herderPos.x, herderPos.y, midRect) <= 2;
	Position target = pastMid ? EXIT_AREA_CENTER : HERDER_WAYPOINT_MID;
	CalypsoDirector::get().steerUnit(bg, active, target);
	checkBranchB(bg);
}

// --------------------------------------------------------------------------- //
// endings
// --------------------------------------------------------------------------- //

void CalypsoPrologueScene::enterEvacOnly(BattlescapeGame *bg, bool announceObjective)
{
	if (_evacOnly || !bg) return;
	SavedBattleGame *save = bg->getSave();
	_evacOnly = true;
	_phase = Ph::Gauntlet; // keeps the reskinned cast-off path enabled
	_gauntletStep = 2;
	_currentVictimId = -1;
	if (save)
	{
		if (BattleUnit *nikos = findUnit(save, _nikosId))
			if (!nikos->isOut())
			{
				if (!nikos->hasScriptedPlayerControl())
					CalypsoDirector::get().handoffToPlayer(bg, nikos);
				_nikosHandedOff = true;
				_nikosFocusPending = true;
			}
	}
	if (announceObjective) radio(STR_PROLOGUE_RADIO_OBJECTIVE);
}

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
	// OutcomeAllTaken never reaches here with a non-empty survivor stash --
	// killEveryoneAboard() (Branch Б) and the "nobody made it aboard" arm
	// path both leave no one alive to snapshot; only OutcomeCastOff
	// (onAbortRequested) ever calls Calypso::stashSurvivor.
	_finishedOutcome = outcome;
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

	// The intended reveal is the Assessor's ambush death, not the later
	// gauntlet-loss bookkeeping in _firstDeathDone.  Flip the persisted gate in
	// the casualty callback so an intervening FOV update cannot publish the
	// marksman early.
	if (id == _assessorId && _phase == Ph::Ambushed)
	{
		revealMarksman(bg);
		return;
	}

	// Amendment #9 (design doc s8 #9): a dead herder buys one Choir turn --
	// the replacement-release turn picks no victim. Counted here (reaction
	// fire on the Choir turn and player fire both arrive through
	// checkForCasualties), consumed in stepGauntlet.
	if (std::find(_herderIds.begin(), _herderIds.end(), id) != _herderIds.end())
	{
		++_herderReprieveTurns;
		checkBranchB(bg);
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

	// Amendment #10 (design doc s8 #10): gauntlet losses among the crew are
	// TAKEN -- queue the body for collection at the next player-turn start.
	// The Assessor is deliberately NOT queued (his corpse stays: the ambush
	// needs it -- demonstration vs collection); ending-context deaths never
	// reach this line (the _endingTriggered early-return above).
	if (_phase == Ph::Gauntlet
		&& (id == _leaderId || id == _nikosId
			|| std::find(_diverIds.begin(), _diverIds.end(), id) != _diverIds.end()))
	{
		_pendingTakenIds.push_back(id);
	}

	if (id == _currentVictimId) _currentVictimId = -1;

	checkBranchB(bg);
}

bool CalypsoPrologueScene::onAbortRequested(BattlescapeState *bs)
{
	if (!bs) return false;
	BattlescapeGame *bg = bs->getBattleGame();
	if (!bg) return false;
	SavedBattleGame *save = bg->getSave();
	if (!save) return false;

	bool anyoneAboard = abortConfirmAvailable(save);
	// While a fallback is pending, before the evacuation order enables cast-off, or with
	// nobody aboard, consume the click without entering vanilla's abort path.
	// Vanilla would set SavedBattleGame::_aborted before finishBattle, poisoning
	// the rolling save even if the director subsequently blocked completion.
	bool castOffAvailable = abortAvailable();
	if (Calypso::consumeAbortRequest(_inert, castOffAvailable, anyoneAboard)) return true;

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

	// D7 survivor stash: snapshot every player soldier that made it aboard,
	// BEFORE the throwaway SavedGame is torn down (the real campaign, created
	// later by finishPrologue, is a totally separate SavedGame -- these
	// Soldier objects will not survive past this battle).
	for (int id : allPlayerIds())
	{
		BattleUnit *u = findUnit(save, id);
		if (!u || u->isOut() || !u->isInExitArea(START_POINT)) continue;
		if (Soldier *geoscapeSoldier = u->getGeoscapeSoldier())
		{
			Calypso::stashSurvivor(geoscapeSoldier->getName(), *geoscapeSoldier->getCurrentStatsEditable());
		}
	}

	// Synchronous endScene is safe here: this is AbortMissionState::btnOkClick,
	// the exact call site vanilla itself invokes finishBattle from.
	_finishedOutcome = OutcomeCastOff;
	CalypsoDirector::get().endScene(bg, OutcomeCastOff);
	return true;
}

bool CalypsoPrologueScene::onUnexpectedFinish(BattlescapeState *bs, bool abort, int *outcome)
{
	if (!bs) return false;
	BattlescapeGame *bg = bs->getBattleGame();
	SavedBattleGame *save = bg ? bg->getSave() : nullptr;
	if (!save) return false;

	bool anyCrewAlive = false;
	for (int id : allPlayerIds())
	{
		BattleUnit *u = findUnit(save, id);
		if (u && !u->isOut()) { anyCrewAlive = true; break; }
	}

	switch (Calypso::decideUnexpectedFinish(_inert, _pendingOutcome >= 0,
		abort, anyCrewAlive, _evacOnly))
	{
		case Calypso::UnexpectedFinishAction::FallbackOutcome:
		{
			// Staging/runtime failure is deterministic, never a passive
			// pseudo-battle. Also recovers old inert saves without pendingOutcome.
			int fallback = _pendingOutcome >= 0 ? _pendingOutcome : OutcomeAllTaken;
			_finishedOutcome = fallback;
			if (outcome) *outcome = fallback;
			return true;
		}
		case Calypso::UnexpectedFinishAction::ConsumeAbort:
			// Valid cast-off already selected a director outcome and never arrives
			// here; any other manual abort must leave the scene unchanged.
			return true;
		case Calypso::UnexpectedFinishAction::EnterEvacOnly:
			// Neutralizing the Choir is not victory. With no hostile actor left to
			// pump the script, hand Nikos over and keep cast-off as the only exit.
			enterEvacOnly(bg, _phase != Ph::Gauntlet);
			return true;
		case Calypso::UnexpectedFinishAction::KeepEvacOnly:
			return true;
		case Calypso::UnexpectedFinishAction::AllTakenOutcome:
			break;
	}

	// If vanilla reached us because the last crew member died before the normal
	// gauntlet detector could arm its ending, convert the same finish into the
	// prologue's all-taken outcome instead of falling into DebriefingState.
	_endingTriggered = true;
	_finishedOutcome = OutcomeAllTaken;
	if (outcome) *outcome = OutcomeAllTaken;
	return true;
}

bool CalypsoPrologueScene::abortStrings(std::string *title, std::string *ok, std::string *cancel)
{
	if (!abortAvailable()) return false;
	if (title)  *title  = "STR_PROLOGUE_CASTOFF_TITLE";
	if (ok)     *ok     = "STR_PROLOGUE_CASTOFF_OK";
	if (cancel) *cancel = "STR_PROLOGUE_CASTOFF_CANCEL";
	return true;
}

bool CalypsoPrologueScene::abortAvailable() const
{
	// The scripted evacuation order is issued only after the Assessor dies and
	// enterEvacOnly()/stepAmbushed() advances into Gauntlet. _firstDeathDone is
	// deliberately unrelated: it tracks a later crew death.
	return Calypso::abortHudEnabled(_inert, _phase == Ph::Gauntlet);
}

bool CalypsoPrologueScene::abortConfirmAvailable(SavedBattleGame *save) const
{
	if (!abortAvailable() || !save) return false;
	bool anyoneAboard = false;
	for (int id : allPlayerIds())
	{
		BattleUnit *u = findUnit(save, id);
		if (u && !u->isOut() && u->isInExitArea(START_POINT)) { anyoneAboard = true; break; }
	}
	return Calypso::castOffConfirmEnabled(abortAvailable(), anyoneAboard);
}

State *CalypsoPrologueScene::makeEndState()
{
	// _finishedOutcome was set immediately before the endScene() call that
	// led here (same call stack: endScene -> finishBattle ->
	// interceptFinishBattle -> makeEndState), so it always reflects the
	// outcome the director is currently intercepting.
	return new CalypsoPrologueEndState(_finishedOutcome);
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

// Amendment #10 (design doc s8 #10, "taken, not corpses"): the scripted drop
// runs the REAL lethal pipeline (beats, morale, sound unchanged) -- making the
// victims mechanically unconscious instead would break "aboard = safe" (an
// unconscious body could be carried to the boat and rescued). The taking is
// the cleanup: remove the corpse BattleItems linked to each queued unit, so
// the body is simply gone when the player's turn starts. Dropped equipment
// stays on the quay by design -- the sea keeps what it takes; gear is not
// what it came for. SavedBattleGame::removeItem handles the tile unlink
// itself (moveToOwner(nullptr) -> Tile::removeItem) and frees the item, so
// the corpse-item list is snapshotted before removal.
void CalypsoPrologueScene::collectTakenBodies(BattlescapeGame *bg)
{
	if (_pendingTakenIds.empty() || !bg) return;
	SavedBattleGame *save = bg->getSave();
	if (!save) return;

	for (int id : _pendingTakenIds)
	{
		BattleUnit *u = findUnit(save, id);
		if (!u) continue;
		std::vector<BattleItem*> corpses;
		for (BattleItem *bi : *save->getItems())
		{
			if (bi->getUnit() == u) corpses.push_back(bi);
		}
		for (BattleItem *bi : corpses)
		{
			save->removeItem(bi);
		}
	}
	_pendingTakenIds.clear();
}

void CalypsoPrologueScene::radio(const std::string &stringId, CalypsoRadioLineKind kind) const
{
	// Instructions are deliberately tagged at their scenario call sites; the
	// UI never guesses semantics from a localization-key naming convention.
	if (kind == CalypsoRadioLineKind::Instruction && !CalypsoTutorial::get().guidanceConfigured()) return;
	CalypsoDirector::get().radioLine(getCurrentGame(), stringId, kind);
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
	writer.write("evacOnly", _evacOnly);
	writer.write("marksmanRevealed", _marksmanRevealed);
	writer.write("nikosHandedOff", _nikosHandedOff);
	writer.write("nikosFocusPending", _nikosFocusPending);
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
	writer.write("herderReprieveTurns", _herderReprieveTurns);
	writer.write("pendingTakenIds", _pendingTakenIds);
}

void CalypsoPrologueScene::load(const YAML::YamlNodeReader &reader)
{
	_phase = (Ph)reader["phase"].readVal<int>((int)Ph::Landing);
	_inert = reader["inert"].readVal<bool>(false);
	_endingTriggered = reader["endingTriggered"].readVal<bool>(false);
	_pendingOutcome = reader["pendingOutcome"].readVal<int>(-1);
	_pendingTaking = reader["pendingTaking"].readVal<bool>(false);
	_evacOnly = reader["evacOnly"].readVal<bool>(false);
	// Pre-fix saves have no explicit flags. Once a save reached Gauntlet the
	// Assessor was necessarily dead and Nikos was supposed to be handed over,
	// so phase is the safe backward-compatible reconstruction rule.
	const bool postAmbush = Calypso::phaseImpliesNikosHandoff(
		_phase == Ph::Gauntlet, _phase == Ph::Ended, _evacOnly);
	_marksmanRevealed = reader["marksmanRevealed"].readVal<bool>(postAmbush);
	_nikosHandedOff = reader["nikosHandedOff"].readVal<bool>(postAmbush);
	_nikosFocusPending = reader["nikosFocusPending"].readVal<bool>(_nikosHandedOff);
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
	_herderReprieveTurns = reader["herderReprieveTurns"].readVal<int>(0);
	_pendingTakenIds = reader["pendingTakenIds"].readVal<std::vector<int>>(std::vector<int>{});
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
