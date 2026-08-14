#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- a beveled panel widget shared by the F34 family
 * adapters (Error / Statistics / Notes ...).
 *
 * It has a REAL bitmap fallback (border + inset fill in the caller's palette
 * theme, drawn in draw()) AND a blit()-level claim skip: when the HD overlay has
 * claimed this panel's visual for the current frame, its blit is skipped so the
 * crisper physical replacement takes over; otherwise it renders logically.
 * Unlike the pilot's invisible placeholder, the badge always has something to
 * show. Defined in a header with inline members so both adapter translation
 * units share ONE definition (no ODR clash, no duplication).
 *
 * Whole-file Emscripten guard (Phase 36).
 */
#ifdef __EMSCRIPTEN__

#include <SDL.h>

#include "../Engine/Surface.h"
#include "CalypsoHdUiOverlay.h"

namespace OpenXcom
{
namespace Calypso
{

class CalypsoBevelPanel : public Surface
{
public:
	CalypsoBevelPanel() : Surface(1, 1, 0, 0) {}
	void setTheme(Uint8 border, Uint8 fill) { _border = border; _fill = fill; }

	void blit(SDL_Surface* surface) override
	{
		if (CalypsoHdUiOverlay::instance().widgetClaimed(this,
				CalypsoHdUiOverlay::instance().frameId()))
			return;
		Surface::blit(surface);
	}

	void draw() override
	{
		Surface::draw();
		SDL_Rect r{ 0, 0, getWidth(), getHeight() };
		drawRect(&r, _border);
		SDL_Rect inner{ 2, 2, getWidth() - 4, getHeight() - 4 };
		if (inner.w > 0 && inner.h > 0) drawRect(&inner, _fill);
	}

private:
	Uint8 _border = 1;
	Uint8 _fill = 0;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
