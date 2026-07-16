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
/* Calypso — Emscripten-only hostile-turn pacing bodies (Phase 43.1). */
#ifdef __EMSCRIPTEN__

#include <emscripten.h>

#include "../Battlescape/BattlescapeGame.h"
#include "../Battlescape/BattlescapeState.h"
#include "../Engine/CalypsoAlienPacing.h"
#include "../Engine/Game.h"
#include "../Engine/Logger.h"
#include "../Engine/Options.h"
#include "../Engine/Timer.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

void BattlescapeState::calypsoAdvanceAlienPacing(Game *calypsoGame)
{
	/* Fixed internal drain: advance at most three ADDITIONAL queued action
	 * states after the normal think/timers. It is not configurable and does not
	 * call the planner more than once per Game iteration. Scheduling can still
	 * change when later iterations happen, so same-seed browser QA owns the
	 * behavior verdict. The loop revalidates lifecycle and the full production
	 * gate before every handleState() call. */
	const int calypsoAdditionalStates = calypsoAlienPacingAdditionalStateCount(
		_gameTimer->isRunning(),
		_popups.empty(),
		_save->getSide() == FACTION_HOSTILE,
		Options::battleAlienSpeed,
		calypsoGame->getMod()->getAISharedFields(),
		calypsoGame->getMod()->getAIEvalBudget(),
		_battleGame->isBusy());
	for (int i = 0; i < calypsoAdditionalStates
		&& calypsoGame->isState(this)
		&& _gameTimer->isRunning()
		&& _popups.empty()
		&& _save->getSide() == FACTION_HOSTILE
		&& Options::battleAlienSpeed == 1
		&& calypsoGame->getMod()->getAISharedFields()
		&& calypsoGame->getMod()->getAIEvalBudget() > 0
		&& _battleGame->isBusy(); ++i)
	{
		_battleGame->handleState();
	}

	if (!calypsoGame->isState(this))
		return;

	/* The pure gate requires Quick Mode plus both deterministic AI feature
	 * knobs; feature-off/default-speed paths never lease the fast scheduler.
	 * Game revalidates requester/state after tutorial pumping. At 75 ms it
	 * requests RAF, and fast mode cannot resume until an actual render. */
	const bool calypsoFastMainLoopEligible = calypsoAlienPacingEligible(
		_gameTimer->isRunning(),
		_popups.empty(),
		_save->getSide() == FACTION_HOSTILE,
		Options::battleAlienSpeed,
		calypsoGame->getMod()->getAISharedFields(),
		calypsoGame->getMod()->getAIEvalBudget());
	if (calypsoFastMainLoopEligible)
	{
		calypsoGame->requestFastMainLoop(this);
	}
}

void Game::calypsoApplyFastMainLoopTiming(State *requester, bool renderedThisIteration)
{
	/* A valid requester leases a setImmediate-driven next tick; every other path
	 * restores requestAnimationFrame. Validate the top state after all logic and
	 * rendering, immediately before changing the scheduler for the next iteration.
	 * At 75 ms since the last actual render this requests RAF. The request is not
	 * itself a hard presentation deadline; fast mode cannot resume until a later
	 * RAF iteration actually renders. */
	const bool leaseValid = !_quit && _init && requester != 0 && isState(requester);
	const bool fastRequested = leaseValid
		&& calypsoAlienPacingBeforeRafThreshold(SDL_GetTicks(), _fastMainLoopLastRenderMs)
		&& (_fastMainLoopApplied || renderedThisIteration);
	if (fastRequested != _fastMainLoopApplied)
	{
		const int timingResult = emscripten_set_main_loop_timing(
			fastRequested ? EM_TIMING_SETIMMEDIATE : EM_TIMING_RAF,
			fastRequested ? 0 : 1);
		if (timingResult == 0)
		{
			_fastMainLoopApplied = fastRequested;
		}
		else
		{
			Log(LOG_ERROR) << "Calypso: failed to change main-loop timing; keeping tracked mode unchanged";
		}
	}
}

/**
 * Requests a one-iteration fast-loop lease for requester. iterate() consumes
 * the request unconditionally and honors it only while requester remains the
 * top state after tutorial pumping and at the end-of-iteration timing switch.
 */
void Game::requestFastMainLoop(State *requester)
{
	_fastMainLoopRequester = requester;
}

} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
