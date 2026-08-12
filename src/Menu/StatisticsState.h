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

namespace Calypso
{
class CalypsoStatisticsUi;
}

class TextButton;
class Window;
class Text;
class TextList;
class Action;

/**
 * Statistics window that shows up
 * at the end of the game.
 */
class StatisticsState : public State
{
friend class Calypso::CalypsoStatisticsUi;
private:
	TextButton *_btnOk;
	Window *_window;
	Text *_txtTitle;
	TextList *_lstStats;
#ifdef __EMSCRIPTEN__
	/// Phase 46.2-HD: F34.Statistics on the shared HD UI overlay
	/// (CalypsoStatisticsUi). `_hdLayout` is the fail-safe gate
	/// (Mod::isHdUiFamilyEnabled("F34")); every field below stays null/false
	/// when it is false, so a disabled/missing HD pack leaves this state
	/// byte-for-byte the legacy statistics screen.
	bool _hdLayout = false;
	bool _hdWideLayout = false;
	TextButton *_btnScrollUp = nullptr;
	TextButton *_btnScrollDown = nullptr;
	Surface *_hdHeaderPanel = nullptr;
	Surface *_hdListPanel = nullptr;
	Surface *_hdReturnPanel = nullptr;
	Surface *_hdFooterPanel = nullptr;
	Calypso::CalypsoStatisticsUi *_hdAdapter = nullptr;
#endif

	// Sums a list of numbers.
	template <typename T>
	T sumVector(const std::vector<T> &vec) const;
public:
	/// Creates the New Game state.
	StatisticsState();
	/// Cleans up the New Game state.
	~StatisticsState();
	/// Gets the save stats.
	void listStats();
	/// Let the state know the window has been resized.
	void resize(int &dX, int &dY) override;
	/// Handler for state input.
	void handle(Action *action) override;
	/// Handler for the F34 statistics scroll-up control.
	void btnScrollUpClick(Action *action);
	/// Handler for the F34 statistics scroll-down control.
	void btnScrollDownClick(Action *action);
	/// Handler for clicking the Ok button.
	void btnOkClick(Action *action);
};

}
