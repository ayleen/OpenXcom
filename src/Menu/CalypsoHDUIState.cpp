/*
 * Phase 6a.2 — Calypso HD UI demo state.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHDUIState.h"
#include "../Engine/Game.h"
#include "../Engine/Surface.h"
#include "../Engine/Action.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{

CalypsoHDUIState::CalypsoHDUIState()
    : _window(nullptr), _title(nullptr), _body(nullptr),
      _btnOk(nullptr), _frames(0)
{
	// Centre a 480×160 dialog on the 640×400 screen.
	const int wx = (640 - 480) / 2;   // 80
	const int wy = (400 - 160) / 2;   // 120

	_window  = new Window(this, 480, 160, wx, wy);
	_title   = new Text(440, 20, wx + 20, wy + 16);
	_body    = new Text(440, 60, wx + 20, wy + 46);
	_btnOk   = new TextButton(120, 24, wx + 180, wy + 122);

	setStandardPalette("PAL_GEOSCAPE");
	add(_window, "window", "geoscape");
	add(_title,  "text",   "geoscape");
	add(_body,   "text",   "geoscape");
	add(_btnOk,  "button", "geoscape");

	// HD panel background — auto-promotes Window to ARGB.
	Surface *panel = _game->getMod()->getSurface("CALYPSO_UI_PANEL_HD", false);
	if (panel)
		_window->setBackground(panel);

	// ARGB title text — promotes the Text surface and triggers
	// TextButton ARGB promotion when it has an ARGB child.
	_title->setBig();
	_title->setAlign(ALIGN_CENTER);
	_title->setColorRGB(0xFFFFD080u);  // warm gold, fully opaque
	_title->setText("Calypso HD UI Demo");

	_body->setSmall();
	_body->setWordWrap(true);
	_body->setColorRGB(0xFFD0E8FFu);   // light blue, fully opaque
	_body->setText("Phase 6a.2: ARGB UI subtree.\n"
	               "Window promoted to ARGB; text rendered\n"
	               "with 32-bit color (setColorRGB).");

	_btnOk->setColor(192);
	_btnOk->setTextColorRGB(0xFFE0E0E0u);  // light gray ARGB
	_btnOk->setText("OK");
	_btnOk->onMouseClick((ActionHandler)&CalypsoHDUIState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&CalypsoHDUIState::btnOkClick,
	                        SDLK_RETURN);
	_btnOk->onKeyboardPress((ActionHandler)&CalypsoHDUIState::btnOkClick,
	                        SDLK_ESCAPE);
}

CalypsoHDUIState::~CalypsoHDUIState()
{
}

void CalypsoHDUIState::init()
{
	State::init();
	_frames = 0;
}

void CalypsoHDUIState::think()
{
	State::think();
	// Auto-dismiss after 180 frames (~3 s) if no interaction.
	if (++_frames >= 180)
		_game->popState();
}

void CalypsoHDUIState::btnOkClick(Action *)
{
	_game->popState();
}

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
