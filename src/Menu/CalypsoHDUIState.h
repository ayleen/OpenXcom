#pragma once
/*
 * Phase 6a.2 — Calypso HD UI demo state.
 *
 * Demonstrates the ARGB UI subtree introduced in 6a.2:
 *   - Window auto-promoted to ARGB (via promoteToARGB()) hosting an HD panel bg
 *   - Text label rendered with setColorRGB() (ARGB glyph path)
 *   - TextButton auto-promoted to ARGB when its text child is ARGB
 *
 * Emscripten-only. Pushed after CalypsoSplashState when
 * calypso-hd-demo mod is active.
 */
#ifdef __EMSCRIPTEN__

#include "../Engine/State.h"

namespace OpenXcom
{

class Window;
class Text;
class TextButton;

class CalypsoHDUIState : public State
{
private:
	Window     *_window;
	Text       *_title;
	Text       *_body;
	TextButton *_btnOk;
	int         _frames;

public:
	CalypsoHDUIState();
	~CalypsoHDUIState();
	void init()  override;
	void think() override;
	void btnOkClick(Action *action);
};

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
