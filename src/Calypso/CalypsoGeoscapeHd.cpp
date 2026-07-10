#ifdef __EMSCRIPTEN__

#include "CalypsoGeoscapeHd.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Screen.h"
#include "../Engine/Surface.h"
#include "../Engine/InteractiveSurface.h"
#include "../Interface/TextButton.h"
#include "../Interface/Text.h"
#include "../Mod/Mod.h"
#include <algorithm>
#include <SDL.h>

namespace OpenXcom
{

void CalypsoGeoscapeHd::applyTtf(GeoscapeState *s)
{
	TTFFont *fontHud = s->_game->getMod()->getTTFFont("FONT_HD_HUD", false);
	TTFFont *fontNumbers = s->_game->getMod()->getTTFFont("FONT_HD_NUMBERS", false);

	if (fontHud)
	{
		TextButton *buttons[] = {
			s->_btnIntercept, s->_btnBases, s->_btnGraphs, s->_btnUfopaedia, s->_btnOptions, s->_btnFunding,
			s->_timeSpeed,
			s->_btn5Secs, s->_btn1Min, s->_btn5Mins, s->_btn30Mins, s->_btn1Hour, s->_btn1Day,
			s->_sideTop, s->_sideBottom
		};
		for (TextButton *b : buttons)
		{
			if (b) b->setTTFFont(fontHud);
		}
	}

	if (fontNumbers)
	{
		Text *texts[] = {
			s->_txtFunds, s->_txtHour, s->_txtHourSep, s->_txtMin, s->_txtMinSep,
			s->_txtSec, s->_txtWeekday, s->_txtDay, s->_txtMonth, s->_txtYear
		};
		for (Text *t : texts)
		{
			if (t) t->setTTFFont(fontNumbers);
		}
	}
}

void CalypsoGeoscapeHd::layout(GeoscapeState *s)
{
	const int screenWidth = Options::baseXGeoscape;
	const int screenHeight = Options::baseYGeoscape;
	const int sc = std::max(1, Options::baseYGeoscape / 400);

	// Sidebar column (mirrors GeoscapeState ctor geometry, every design offset
	// multiplied by sc). The globe (_bg / the rest of _sideLine) is untouched;
	// only the right-edge column moves and widens.
	s->_sideLine->setWidth(64 * sc);
	s->_sideLine->setX(screenWidth - 64 * sc);

	s->_sidebar->setWidth(64 * sc);
	s->_sidebar->setHeight(200 * sc);
	s->_sidebar->setX(screenWidth - 64 * sc);
	s->_sidebar->setY(screenHeight / 2 - 100 * sc);

	s->_btnIntercept->setWidth(63 * sc); s->_btnIntercept->setHeight(11 * sc);
	s->_btnIntercept->setX(screenWidth - 63 * sc); s->_btnIntercept->setY(screenHeight / 2 - 100 * sc);
	s->_btnBases->setWidth(63 * sc); s->_btnBases->setHeight(11 * sc);
	s->_btnBases->setX(screenWidth - 63 * sc); s->_btnBases->setY(screenHeight / 2 - 88 * sc);
	s->_btnGraphs->setWidth(63 * sc); s->_btnGraphs->setHeight(11 * sc);
	s->_btnGraphs->setX(screenWidth - 63 * sc); s->_btnGraphs->setY(screenHeight / 2 - 76 * sc);
	s->_btnUfopaedia->setWidth(63 * sc); s->_btnUfopaedia->setHeight(11 * sc);
	s->_btnUfopaedia->setX(screenWidth - 63 * sc); s->_btnUfopaedia->setY(screenHeight / 2 - 64 * sc);
	s->_btnOptions->setWidth(63 * sc); s->_btnOptions->setHeight(11 * sc);
	s->_btnOptions->setX(screenWidth - 63 * sc); s->_btnOptions->setY(screenHeight / 2 - 52 * sc);
	s->_btnFunding->setWidth(63 * sc); s->_btnFunding->setHeight(11 * sc);
	s->_btnFunding->setX(screenWidth - 63 * sc); s->_btnFunding->setY(screenHeight / 2 - 40 * sc);

	s->_btn5Secs->setWidth(31 * sc); s->_btn5Secs->setHeight(13 * sc);
	s->_btn5Secs->setX(screenWidth - 63 * sc); s->_btn5Secs->setY(screenHeight / 2 + 12 * sc);
	s->_btn1Min->setWidth(31 * sc); s->_btn1Min->setHeight(13 * sc);
	s->_btn1Min->setX(screenWidth - 31 * sc); s->_btn1Min->setY(screenHeight / 2 + 12 * sc);
	s->_btn5Mins->setWidth(31 * sc); s->_btn5Mins->setHeight(13 * sc);
	s->_btn5Mins->setX(screenWidth - 63 * sc); s->_btn5Mins->setY(screenHeight / 2 + 26 * sc);
	s->_btn30Mins->setWidth(31 * sc); s->_btn30Mins->setHeight(13 * sc);
	s->_btn30Mins->setX(screenWidth - 31 * sc); s->_btn30Mins->setY(screenHeight / 2 + 26 * sc);
	s->_btn1Hour->setWidth(31 * sc); s->_btn1Hour->setHeight(13 * sc);
	s->_btn1Hour->setX(screenWidth - 63 * sc); s->_btn1Hour->setY(screenHeight / 2 + 40 * sc);
	s->_btn1Day->setWidth(31 * sc); s->_btn1Day->setHeight(13 * sc);
	s->_btn1Day->setX(screenWidth - 31 * sc); s->_btn1Day->setY(screenHeight / 2 + 40 * sc);

	s->_btnRotateLeft->setWidth(12 * sc); s->_btnRotateLeft->setHeight(10 * sc);
	s->_btnRotateLeft->setX(screenWidth - 61 * sc); s->_btnRotateLeft->setY(screenHeight / 2 + 76 * sc);
	s->_btnRotateRight->setWidth(12 * sc); s->_btnRotateRight->setHeight(10 * sc);
	s->_btnRotateRight->setX(screenWidth - 37 * sc); s->_btnRotateRight->setY(screenHeight / 2 + 76 * sc);
	s->_btnRotateUp->setWidth(13 * sc); s->_btnRotateUp->setHeight(12 * sc);
	s->_btnRotateUp->setX(screenWidth - 49 * sc); s->_btnRotateUp->setY(screenHeight / 2 + 62 * sc);
	s->_btnRotateDown->setWidth(13 * sc); s->_btnRotateDown->setHeight(12 * sc);
	s->_btnRotateDown->setX(screenWidth - 49 * sc); s->_btnRotateDown->setY(screenHeight / 2 + 87 * sc);
	s->_btnZoomIn->setWidth(23 * sc); s->_btnZoomIn->setHeight(23 * sc);
	s->_btnZoomIn->setX(screenWidth - 25 * sc); s->_btnZoomIn->setY(screenHeight / 2 + 56 * sc);
	s->_btnZoomOut->setWidth(13 * sc); s->_btnZoomOut->setHeight(17 * sc);
	s->_btnZoomOut->setX(screenWidth - 20 * sc); s->_btnZoomOut->setY(screenHeight / 2 + 82 * sc);

	// Side fillers: same "gap above/below the panel" formula as the ctor,
	// with the fixed height offset scaled by sc too so it grows with the panel.
	const int fillerHeight = ((screenHeight - Screen::ORIGINAL_HEIGHT) / 2 + 10) * sc;
	s->_sideTop->setWidth(63 * sc); s->_sideTop->setHeight(fillerHeight);
	s->_sideTop->setX(screenWidth - 63 * sc); s->_sideTop->setY(s->_sidebar->getY() - fillerHeight - 1);
	s->_sideBottom->setWidth(63 * sc); s->_sideBottom->setHeight(fillerHeight);
	s->_sideBottom->setX(screenWidth - 63 * sc); s->_sideBottom->setY(s->_sidebar->getY() + s->_sidebar->getHeight() + 1);

	s->_txtFunds->setWidth(59 * sc); s->_txtFunds->setHeight(8 * sc);
	s->_txtFunds->setX(screenWidth - 61 * sc); s->_txtFunds->setY(screenHeight / 2 - 27 * sc);
	s->_txtHour->setWidth(20 * sc); s->_txtHour->setHeight(16 * sc);
	s->_txtHour->setX(screenWidth - 61 * sc); s->_txtHour->setY(screenHeight / 2 - 26 * sc);
	s->_txtHourSep->setWidth(4 * sc); s->_txtHourSep->setHeight(16 * sc);
	s->_txtHourSep->setX(screenWidth - 41 * sc); s->_txtHourSep->setY(screenHeight / 2 - 26 * sc);
	s->_txtMin->setWidth(20 * sc); s->_txtMin->setHeight(16 * sc);
	s->_txtMin->setX(screenWidth - 37 * sc); s->_txtMin->setY(screenHeight / 2 - 26 * sc);
	s->_txtMinSep->setWidth(4 * sc); s->_txtMinSep->setHeight(16 * sc);
	s->_txtMinSep->setX(screenWidth - 17 * sc); s->_txtMinSep->setY(screenHeight / 2 - 26 * sc);
	s->_txtSec->setWidth(11 * sc); s->_txtSec->setHeight(8 * sc);
	s->_txtSec->setX(screenWidth - 13 * sc); s->_txtSec->setY(screenHeight / 2 - 20 * sc);
	s->_txtWeekday->setWidth(59 * sc); s->_txtWeekday->setHeight(8 * sc);
	s->_txtWeekday->setX(screenWidth - 61 * sc); s->_txtWeekday->setY(screenHeight / 2 - 13 * sc);
	s->_txtDay->setWidth(29 * sc); s->_txtDay->setHeight(8 * sc);
	s->_txtDay->setX(screenWidth - 61 * sc); s->_txtDay->setY(screenHeight / 2 - 6 * sc);
	s->_txtMonth->setWidth(29 * sc); s->_txtMonth->setHeight(8 * sc);
	s->_txtMonth->setX(screenWidth - 32 * sc); s->_txtMonth->setY(screenHeight / 2 - 6 * sc);
	s->_txtYear->setWidth(59 * sc); s->_txtYear->setHeight(8 * sc);
	s->_txtYear->setX(screenWidth - 61 * sc); s->_txtYear->setY(screenHeight / 2 + 1 * sc);

	// HD plate art: new ids only (never hd:true on GEOBORD.SCR/ALTGEOBORD.SCR —
	// that leaked into the globe palette in the main-menu incident). Blit once
	// into the existing member surfaces; no per-frame allocation.
	Surface *plate = s->_game->getMod()->getSurface("CALYPSO_GEOBORD_HD", false);
	if (plate && plate->getSurface() && s->_sidebar->getSurface())
	{
		SDL_SetSurfaceBlendMode(plate->getSurface(), SDL_BLENDMODE_NONE);
		SDL_Rect dst{ 0, 0, s->_sidebar->getWidth(), s->_sidebar->getHeight() };
		SDL_BlitScaled(plate->getSurface(), nullptr, s->_sidebar->getSurface(), &dst);
		s->_sidebar->setRedraw(false);
	}

	Surface *line = s->_game->getMod()->getSurface("CALYPSO_GEOBORD_LINE_HD", false);
	if (line && line->getSurface() && s->_sideLine->getSurface())
	{
		SDL_SetSurfaceBlendMode(line->getSurface(), SDL_BLENDMODE_NONE);
		const int tile = 64 * sc;
		for (int y = 0; y < s->_sideLine->getHeight(); y += tile)
		{
			SDL_Rect dst{ 0, y, s->_sideLine->getWidth(), std::min(tile, s->_sideLine->getHeight() - y) };
			SDL_BlitScaled(line->getSurface(), nullptr, s->_sideLine->getSurface(), &dst);
		}
		s->_sideLine->setRedraw(false);
	}
}

}

#endif
