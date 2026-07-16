/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/* Calypso — Emscripten-only AI regression harness exports (Phase 43). */
#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <exception>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Logger.h"
#include "../Engine/Options.h"
#include "../Mod/Mod.h"
#include "../Savegame/FactionTurnCache.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/SavedGame.h"

using namespace OpenXcom;

extern "C" {

/* One-shot deterministic AI failure probe for regression scenarios.  Normal
 * gameplay never arms it; the harness opts in after loading a scenario save.
 * ProjectileFlyBState consumes an exact unit/action match before the action
 * spends TU or ammo, then follows the production failure-memory path. */
static int s_aiFailureProbeUnitId = -1;
static int s_aiFailureProbeAction = -1;
static bool s_aiFailureProbeArmed = false;

EMSCRIPTEN_KEEPALIVE
void calypso_arm_ai_failure_probe(int unitId, int actionType)
{
	s_aiFailureProbeUnitId = unitId;
	s_aiFailureProbeAction = actionType;
	s_aiFailureProbeArmed = true;
}

int calypso_consume_ai_failure_probe(int unitId, int actionType)
{
	if (!s_aiFailureProbeArmed || unitId != s_aiFailureProbeUnitId
		|| (s_aiFailureProbeAction != -1 && actionType != s_aiFailureProbeAction))
	{
		return 0;
	}
	s_aiFailureProbeArmed = false;
	return 1;
}

/* Re-apply the dev-only AI trace opt-in after callMain unwinds to JS and before
 * subsequent engine frames process a deterministic harness scenario. */
EMSCRIPTEN_KEEPALIVE
void calypso_set_trace_ai(int enabled)
{
	OpenXcom::Options::traceAI = enabled != 0;
}

/* Enable the decision-affecting failure-memory gate for an explicit regression
 * scenario without changing the shipped ruleset default. */
EMSCRIPTEN_KEEPALIVE
int calypso_set_ai_failure_memory(int enabled)
{
	Game *g = getCurrentGame();
	SavedBattleGame *battle = g && g->getSavedGame() ? g->getSavedGame()->getSavedBattle() : nullptr;
	Mod *gameMod = g ? g->getMod() : nullptr;
	Mod *battleMod = battle ? const_cast<Mod *>(battle->getMod()) : nullptr;
	if (!gameMod || !battleMod) return 0;
	gameMod->setAIFailureMemoryForHarness(enabled != 0);
	battleMod->setAIFailureMemoryForHarness(enabled != 0);
	return gameMod->getAIFailureMemory() == (enabled != 0)
		&& battleMod->getAIFailureMemory() == (enabled != 0) ? 1 : 0;
}

/* Phase 43.1 QA harness: override the shared per-faction spatial fields for one
 * explicit regression scenario after the shipped ruleset has loaded.
 * The three calypso_set_ai_* exports below mirror calypso_set_ai_failure_memory
 * exactly (apply to BOTH the Game-level Mod and the active SavedBattleGame's Mod
 * so reads from either holder agree) and are intended to be called by the JS
 * harness after callMain + ruleset load, before the alien turn runs. They accept
 * the schema sentinels false/0 as well as armed values, so QA can explicitly
 * exercise either profile without mutating the ruleset bytes. */
EMSCRIPTEN_KEEPALIVE
int calypso_set_ai_shared_fields(int enabled)
{
	Game *g = getCurrentGame();
	SavedBattleGame *battle = g && g->getSavedGame() ? g->getSavedGame()->getSavedBattle() : nullptr;
	Mod *gameMod = g ? g->getMod() : nullptr;
	Mod *battleMod = battle ? const_cast<Mod *>(battle->getMod()) : nullptr;
	if (!gameMod || !battleMod) return 0;
	gameMod->setAISharedFieldsForHarness(enabled != 0);
	battleMod->setAISharedFieldsForHarness(enabled != 0);
	return gameMod->getAISharedFields() == (enabled != 0)
		&& battleMod->getAISharedFields() == (enabled != 0) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int calypso_set_ai_eval_budget(int budget)
{
	Game *g = getCurrentGame();
	SavedBattleGame *battle = g && g->getSavedGame() ? g->getSavedGame()->getSavedBattle() : nullptr;
	Mod *gameMod = g ? g->getMod() : nullptr;
	Mod *battleMod = battle ? const_cast<Mod *>(battle->getMod()) : nullptr;
	if (!gameMod || !battleMod) return 0;
	const int clamped = budget < 0 ? 0 : budget;
	gameMod->setAIEvalBudgetForHarness(clamped);
	battleMod->setAIEvalBudgetForHarness(clamped);
	return gameMod->getAIEvalBudget() == clamped
		&& battleMod->getAIEvalBudget() == clamped ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int calypso_set_ai_turn_budget_ms(int budgetMs)
{
	Game *g = getCurrentGame();
	SavedBattleGame *battle = g && g->getSavedGame() ? g->getSavedGame()->getSavedBattle() : nullptr;
	Mod *gameMod = g ? g->getMod() : nullptr;
	Mod *battleMod = battle ? const_cast<Mod *>(battle->getMod()) : nullptr;
	if (!gameMod || !battleMod) return 0;
	const int clamped = budgetMs < 0 ? 0 : budgetMs;
	gameMod->setAITurnBudgetMsForHarness(clamped);
	battleMod->setAITurnBudgetMsForHarness(clamped);
	return gameMod->getAITurnBudgetMs() == clamped
		&& battleMod->getAITurnBudgetMs() == clamped ? 1 : 0;
}

/* Phase 43.1 off-from-load proof: read the values from the Mod attached to the
 * active SavedBattleGame after callMain + harness settle. These exports are
 * deliberately read-only; -1 means there is no current battle Mod yet. */
static const Mod *calypso_current_battle_mod()
{
	Game *g = getCurrentGame();
	SavedBattleGame *battle = g && g->getSavedGame() ? g->getSavedGame()->getSavedBattle() : nullptr;
	return battle ? battle->getMod() : nullptr;
}

EMSCRIPTEN_KEEPALIVE
int calypso_get_ai_shared_fields()
{
	const Mod *battleMod = calypso_current_battle_mod();
	return battleMod ? (battleMod->getAISharedFields() ? 1 : 0) : -1;
}

EMSCRIPTEN_KEEPALIVE
int calypso_get_ai_eval_budget()
{
	const Mod *battleMod = calypso_current_battle_mod();
	return battleMod ? battleMod->getAIEvalBudget() : -1;
}

EMSCRIPTEN_KEEPALIVE
int calypso_get_ai_turn_budget_ms()
{
	const Mod *battleMod = calypso_current_battle_mod();
	return battleMod ? battleMod->getAITurnBudgetMs() : -1;
}

/* Phase 43.1 QA harness: arm OXCE's supported alien Quick Mode semantics for
 * the PORT/Superhuman DoD run, without changing the production default or the
 * off-capture comparison. Options::battleAlienSpeed is the SAME knob Ctrl+S
 * toggles in BattlescapeState::keyRelease for the alien side: it drives
 * setStateInterval (UnitWalk/Fall/TurnBState) — lower is faster, and 1 is the
 * value Ctrl+S assigns when Quick Mode activates. The root architecture
 * decision for this run is that the engine faction timer INCLUDES action
 * presentation, so the DoD drive runs the alien turn at the Quick Mode speed
 * (Options::battleAlienSpeed=1) exactly as a Ctrl+S player would.
 *
 * The export clamps the input to the same [1,40] range the Ctrl+S restore guard
 * accepts (battleAlienSpeedOrig >= 1 && <= 40 in BattlescapeState.cpp) and
 * returns the applied value so the JS harness can assert the clamp held. It
 * does NOT touch battleAlienSpeedOrig: the harness owns the knob for the whole
 * run, so the save/restore dance Ctrl+S uses would only interfere with a later
 * manual toggle. Production behaviour is unchanged — the export is
 * __EMSCRIPTEN__-gated and only the harness calls it; the shipped default (30)
 * is restored on the next normal options.cfg load. */
EMSCRIPTEN_KEEPALIVE
int calypso_set_alien_speed(int speed)
{
	const int clamped = speed < 1 ? 1 : (speed > 40 ? 40 : speed);
	Options::battleAlienSpeed = clamped;
	return Options::battleAlienSpeed;
}

/* ---- Phase 43.1 QA: occupancy save/reload roundtrip harness exports ----------
 *
 * These five exports let a Playwright regression script seed a known sparse
 * occupancy state for a faction (the hostile side), advance its decay marker to
 * a known turn, trigger a REAL production save (SavedGame::save), and — after a
 * fresh browser context reloads that .sav through the normal harness
 * `-load` → SavedGame/SavedBattleGame::load path — read the exact cells +
 * lastAdvancedTurn back to verify they survived the REAL production reload
 * (SavedBattleGame::load is the authoritative deserializer; these exports never
 * substitute a mirrored parser or a direct setOccupancyState call for it).
 *
 * They NEVER run in normal gameplay: the JS QA harness is the only caller, they
 * are compiled only under __EMSCRIPTEN__, and they touch no production default.
 * The seeding path (spikeFactionOccupancy) is itself ai.sharedFields-gated in
 * production, so a script must call calypso_set_ai_shared_fields(1) first; the
 * advance/read exports operate on the FactionTurnCache directly so the test can
 * arm a deterministic marker without driving a full faction turn.
 */

EMSCRIPTEN_KEEPALIVE
int calypso_spike_occupancy(int faction, int x, int y, int z, int amount)
{
	Game *g = getCurrentGame();
	SavedBattleGame *battle = g && g->getSavedGame() ? g->getSavedGame()->getSavedBattle() : nullptr;
	if (!battle) return 0;
	// Production spike (SavedBattleGame::spikeFactionOccupancy) validates faction,
	// map bounds, and the ai.sharedFields gate internally; it is a silent no-op when
	// the gate is off, so a script that forgot to arm sharedFields reads 0 back and
	// fails loudly on the subsequent assertion.
	battle->spikeFactionOccupancy(static_cast<UnitFaction>(faction), Position(x, y, z), amount);
	return 1;
}

EMSCRIPTEN_KEEPALIVE
int calypso_advance_occupancy(int faction, int turn)
{
	Game *g = getCurrentGame();
	SavedBattleGame *battle = g && g->getSavedGame() ? g->getSavedGame()->getSavedBattle() : nullptr;
	if (!battle) return 0;
	FactionTurnCache *cache = battle->getFactionTurnCache(static_cast<UnitFaction>(faction));
	if (!cache) return 0;
	// Direct cache advance (FactionTurnCache::advanceOccupancyToTurn) using the live
	// mod's decay knobs + the fixed 1000 OccupancyField scale. Idempotent per its
	// contract: first call arms the marker without decaying, repeats are no-ops.
	cache->advanceOccupancyToTurn(turn,
		battle->getMapSizeX(), battle->getMapSizeY(), battle->getMapSizeZ(),
		battle->getMod() ? battle->getMod()->getAIOccupancyRetainPercent() : 75,
		battle->getMod() ? battle->getMod()->getAIOccupancySpreadPercent() : 25,
		1000);
	return 1;
}

EMSCRIPTEN_KEEPALIVE
int calypso_read_occupancy(int faction, int x, int y, int z)
{
	Game *g = getCurrentGame();
	SavedBattleGame *battle = g && g->getSavedGame() ? g->getSavedGame()->getSavedBattle() : nullptr;
	if (!battle) return 0;
	const FactionTurnCache *cache = battle->getFactionTurnCache(static_cast<UnitFaction>(faction));
	if (!cache) return 0;
	return cache->getOccupancyField().valueAt(Position(x, y, z));
}

EMSCRIPTEN_KEEPALIVE
int calypso_occupancy_last_turn(int faction)
{
	Game *g = getCurrentGame();
	SavedBattleGame *battle = g && g->getSavedGame() ? g->getSavedGame()->getSavedBattle() : nullptr;
	if (!battle) return -2;   // distinct from the valid -1 disarmed marker
	const FactionTurnCache *cache = battle->getFactionTurnCache(static_cast<UnitFaction>(faction));
	if (!cache) return -2;
	return cache->getOccupancyLastAdvancedTurn();
}

/* Trigger a REAL production save (SavedGame::save) of the live game to
 * `<masterUserFolder>/<filename>` (i.e. /user/<master>/<filename> in MEMFS).
 * The QA occupancy script reads the bytes back via Module.FS.readFile (which
 * reads MEMFS synchronously) and ships them to Node, which writes a temp .sav
 * a SECOND fresh browser context then loads through the normal harness
 * `-load` → SavedGame/SavedBattleGame::load path — the real reload, not an
 * in-memory mirror. SavedGame::save also queues an async IDBFS syncfs as a side
 * effect; the QA flow does not rely on it (it reads MEMFS directly). Returns 1
 * on success, 0 on any failure (no live game, empty filename, save throw). */
EMSCRIPTEN_KEEPALIVE
int calypso_trigger_save(const char *filename)
{
	Game *g = getCurrentGame();
	if (!g || !g->getSavedGame() || !g->getMod() || !filename || !*filename) return 0;
	try
	{
		g->getSavedGame()->save(std::string(filename), g->getMod());
		return 1;
	}
	catch (const std::exception &e)
	{
		Log(LOG_ERROR) << "calypso_trigger_save: " << e.what();
		return 0;
	}
}

} /* extern "C" */

#endif /* __EMSCRIPTEN__ */
