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
class CalypsoStatisticsStateUi;
}

class TextButton;
class Window;
class Text;
class TextList;
class TTFFont;

/**
 * Statistics window that shows up
 * at the end of the game.
 */
class StatisticsState : public State
{
friend class Calypso::CalypsoStatisticsStateUi;
private:
	TextButton *_btnOk;
	Window *_window;
	Text *_txtTitle;
	TextList *_lstStats;
#ifdef __EMSCRIPTEN__
	TextButton *_btnScrollUp = nullptr;
	TextButton *_btnScrollDown = nullptr;
	bool _hdLayout = false;
	bool _hdWideLayout = false;
	TTFFont *_hdFont = nullptr;
	std::uint64_t _focusGeneration = 0;
	Surface *_hdHeaderPanel = nullptr;
	Surface *_hdListPanel = nullptr;
	Surface *_hdReturnPanel = nullptr;
	Surface *_hdFooterPanel = nullptr;
	Text *_hdRecordLabel = nullptr;
	Text *_hdOutcome = nullptr;
	Text *_hdReturnRole = nullptr;
	Text *_hdReturnDetail = nullptr;
	Text *_hdScrollHint = nullptr;
	Text *_hdFooterStatus = nullptr;
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
	void handle(Action *action) override;
	void resize(int &dX, int &dY) override;
	/// Handler for the F34 statistics scroll-up control.
	void btnScrollUpClick(Action *action);
	/// Handler for the F34 statistics scroll-down control.
	void btnScrollDownClick(Action *action);
	/// Handler for clicking the Ok button.
	void btnOkClick(Action *action);
};

}
