#pragma once
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Phase 41 (Calypso) -- prologue campaign entry/exit. This is the module a
 * fresh New Game either detours through (accept -> throwaway battle -> real
 * campaign) or bypasses entirely (decline, or the mod content is absent).
 * See docs/phases/phase-41-tutorial-mission.md sections 41.5/41.5a and
 * commit4-contract.md decisions D3-D7.
 *
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 */
#ifdef __EMSCRIPTEN__

#include <string>

#include "../Savegame/SavedGame.h" // GameDifficulty
#include "../Mod/Unit.h"           // UnitStats

namespace OpenXcom
{

class Game;

namespace Calypso
{

/// Fixed placeholder soldier names for the throwaway prologue roster. The
/// Leader MUST be created first (BattleUnit ids drive CalypsoPrologueScene's
/// "leader = lowest player id" rule). Easy to swap for real names later.
extern const std::string PROLOGUE_LEADER_NAME;
extern const std::string PROLOGUE_DIVER1_NAME;
extern const std::string PROLOGUE_DIVER2_NAME;

/// Single rolling-autosave slot filename (D6). Shared between
/// CalypsoPrologueScene (writes it every player turn via
/// CalypsoDirector::forceAutosave) and finishPrologue (deletes it).
extern const std::string PROLOGUE_AUTOSAVE_FILENAME;

/// Offered from NewGameState::btnOkClick, BEFORE newSave(). Returns true if
/// it took over (pushed CalypsoPrologueAskState) -- the caller must return
/// immediately and skip the vanilla campaign-creation flow. Returns false
/// (vanilla continues unchanged) when the prologue was already offered once
/// (Options::calypsoPrologueSeen) or the mod content is absent
/// (STR_CALYPSO_PROLOGUE deployment not found -- graceful without commit 5's
/// mod, so the engine never hard-depends on it). `ironman` is the New Game
/// screen's toggle -- stashed alongside the difficulty so neither the decline
/// path nor the post-prologue campaign silently drops the player's choice.
bool maybeOfferPrologue(Game *game, GameDifficulty diff, bool ironman);

/// The difficulty stashed by maybeOfferPrologue -- valid between the ask
/// state's construction and the prologue's resolution (Yes or No). Used by
/// CalypsoPrologueAskState's No handler to run the vanilla tail for the
/// difficulty the player actually picked.
GameDifficulty stashedDifficulty();

/// Runs the vanilla NewGameState::btnOkClick tail (newSave + tutorial hooks +
/// GeoscapeState + base-placement chain) for `diff` and the stashed ironman
/// choice. Duplicated from NewGameState.cpp rather than extracted out of it --
/// NewGameState.cpp's own tail stays untouched for the native/no-prologue
/// path, and this Emscripten-only copy is reused by both the ask state's "No"
/// handler and (via the shared internal tail helper) finishPrologue().
void vanillaNewGameTail(Game *game, GameDifficulty diff);

/// Launches the throwaway prologue battle: fresh SavedGame (monthsPassed
/// stays at the ctor default -1), one STR_TRITON, exactly three fixed
/// soldiers (Leader created first), STR_CALYPSO_PROLOGUE deployment. Mirrors
/// NewBattleState::initSave + btnOkClick (D4). Call site: the ask state's
/// Yes handler (after the not-yet-wired intro-clip trigger, see commit 6).
void launchPrologueBattle(Game *game);

/// Records one surviving player soldier's name + stats at cast-off
/// (OutcomeCastOff only) -- called by CalypsoPrologueScene::onAbortRequested
/// before the throwaway SavedGame is torn down. finishPrologue() injects the
/// stash into the real campaign's starting roster and clears it.
void stashSurvivor(const std::string &name, const UnitStats &stats);

/// Creates the real campaign (mod->newSave), injects any stashed survivors
/// into the starting base roster, deletes the prologue autosave slot, and
/// replicates NewGameState's post-newSave tail. Called by
/// CalypsoPrologueEndState on click/keypress; `outcome` is
/// CalypsoPrologueScene::Outcome (OutcomeAllTaken -> stash is empty, roster
/// untouched).
void finishPrologue(Game *game, int outcome);

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
