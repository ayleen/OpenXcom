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
#include "../Engine/State.h"

namespace OpenXcom
{

class Surface;
class Timer;
class ScannerView;
struct BattleAction;

/**
 * The Scanner User Interface.
 */
class ScannerState : public State
{
	InteractiveSurface *_bg;
	Surface *_scan;
	SDL_Surface *_bgNative = nullptr, *_scanNative = nullptr;  ///< Calypso: 320×200 bg snapshots, bilinear-stretched into the scaled surfaces
	ScannerView *_scannerView;
	BattleAction *_action;
	/// Updates scanner interface.
	void update();
	Timer *_timerAnimate;
	/// Handles Minimap animation.
	void animate();
public:
	/// Creates the ScannerState.
	ScannerState(BattleAction *action);
	~ScannerState();
	/// Calypso: rescale to the logical buffer instead of the base recenter.
	void resize(int &dX, int &dY) override;
	/// Handler for right-clicking anything.
	void handle(Action *action) override;
	/// Handles timers.
	void think() override;
	/// Handler for exiting the state.
	void exitClick(Action *action);
};
}
