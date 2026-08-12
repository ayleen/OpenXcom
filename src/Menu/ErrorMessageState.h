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
#include <string>
#include "../Engine/State.h"

namespace OpenXcom
{

namespace Calypso
{
class CalypsoErrorPopupUi;
}

class TextButton;
class Window;
class Text;

/**
 * Generic window used to display error messages.
 */
class ErrorMessageState : public State
{
friend class Calypso::CalypsoErrorPopupUi;
private:
	TextButton *_btnOk;
	Window *_window;
	Text *_txtMessage;
#ifdef __EMSCRIPTEN__
	/// Phase 46.2-HD: F34.ErrorPopup on the shared HD UI overlay
	/// (CalypsoErrorPopupUi). `_hdLayout` is the fail-safe gate
	/// (Mod::isHdUiFamilyEnabled("F34")); every field below is unused (stays
	/// null/false) when it is false, so a disabled/missing HD pack leaves this
	/// state byte-for-byte the legacy popup. The snapshot-only adapter is driven
	/// at the pre-blit boundary; there is no per-frame feeder Surface.
	bool _hdLayout = false;
	bool _hdWideLayout = false;
	Surface *_hdIconPanel = nullptr;   ///< CalypsoBevelPanel: beveled badge with a bitmap fallback
	Text *_hdIcon = nullptr;
	Text *_hdWarning = nullptr;
	Calypso::CalypsoErrorPopupUi *_hdAdapter = nullptr; ///< owned; registered with the overlay while top
#endif

	void create(const std::string &str, SDL_Color *palette, Uint8 color, const std::string &bg, int bgColor, Uint8 color2);
public:
	/// Creates the Error state.
	ErrorMessageState(const std::string &msg, SDL_Color *palette, Uint8 color, const std::string &bg, int bgColor, Uint8 color2 = 0);
	/// Cleans up the Error state.
	~ErrorMessageState();
	/// Let the state know the window has been resized.
	void resize(int &dX, int &dY) override;
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
};

}
