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
#include "OptionsBaseState.h"

#ifdef __EMSCRIPTEN__
namespace OpenXcom
{
namespace Calypso
{
class CalypsoAbandonPopupUi;
}
}
#endif

namespace OpenXcom
{

class TextButton;
class Window;
class Text;

/**
 * Abandon Game window shown before
 * quitting the game from the Geoscape.
 */
class AbandonGameState : public State
{
private:
	OptionsOrigin _origin;
	TextButton *_btnYes, *_btnNo;
	Window *_window;
	Text *_txtTitle;
#ifdef __EMSCRIPTEN__
	// F33 (Phase 46.2-HD): physical-route state owned by CalypsoAbandonPopupUi.
	friend class Calypso::CalypsoAbandonPopupUi;
	/// Fail-safe gate: physical route is on and this state may use it.
	bool _hdLayout = false;
	/// Last applied layout class (Compact/Wide), recomputed on resize.
	bool _hdWideLayout = false;
	/// HD-only data-loss copy (absent on the logical fallback).
	Text* _hdMessage = nullptr;
	/// HD-only command protocol strip (absent on the logical fallback).
	Text* _hdProtocol = nullptr;
	/// Owned; registered with the overlay while this state is top.
	Calypso::CalypsoAbandonPopupUi* _hdAdapter = nullptr;
#endif
public:
	/// Creates the Abandon Game state.
	AbandonGameState(OptionsOrigin origin);
	/// Cleans up the Abandon Game state.
	~AbandonGameState();
	/// Calypso (Emscripten): rescale to the logical buffer instead of the base recenter.
	void resize(int &dX, int &dY) override;
	/// Handler for clicking the Yes button.
	void btnYesClick(Action *action);
	/// Handler for clicking the No button.
	void btnNoClick(Action *action);
};

}
