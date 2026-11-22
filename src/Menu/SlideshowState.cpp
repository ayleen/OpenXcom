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
#include "SlideshowState.h"
#include "CutsceneState.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <memory>
#include "../Engine/Exception.h"
#include "../Engine/Music.h"
#include "../Engine/Logger.h"
#include "../Calypso/CalypsoHdUiOverlay.h"
#endif
#include "../Engine/FileMap.h"
#include "../Engine/Game.h"
#include "../Engine/InteractiveSurface.h"
#include "../Engine/Screen.h"
#include "../Engine/Timer.h"
#include "../Interface/Text.h"
#include "../Mod/Mod.h"
#include "../Engine/Options.h"
#include "../Interface/Cursor.h"

namespace OpenXcom
{

SlideshowState::SlideshowState(const SlideshowHeader &slideshowHeader, const std::vector<SlideshowSlide> *slideshowSlides)
		: _slideshowHeader(slideshowHeader), _slideshowSlides(slideshowSlides), _curScreen(-1)
{
#ifdef __EMSCRIPTEN__
	_screen = true;
#else
	_wasLetterboxed = CutsceneState::initDisplay();

	// pre-render and queue up all the frames
	for (const auto& def : *_slideshowSlides)
	{
		InteractiveSurface *slide =
			new InteractiveSurface(Screen::ORIGINAL_WIDTH, Screen::ORIGINAL_HEIGHT, 0, 0);
		slide->loadImage(def.imagePath);
		slide->onMouseClick((ActionHandler)&SlideshowState::screenClick);
		slide->onKeyboardPress((ActionHandler)&SlideshowState::screenClick, Options::keyOk);
		slide->onKeyboardPress((ActionHandler)&SlideshowState::screenSkip, Options::keyCancel);
		slide->setVisible(false);
		_slides.push_back(slide);
		setStatePalette(slide->getPalette());
		add(slide);

		// initialize with default rect; may get overridden by
		// category/id definition
		Text *caption = new Text(def.w, def.h, def.x, def.y);
		caption->setColor(def.color);
		caption->setText(tr(def.caption));
		caption->setAlign(def.align);
		caption->setVerticalAlign(def.valign);
		caption->setWordWrap(true);
		caption->setVisible(false);
		_captions.push_back(caption);
		add(caption);
	}

	centerAllSurfaces();

	int transitionSeconds = _slideshowHeader.transitionSeconds;
	if (_slideshowSlides->front().transitionSeconds > 0)
		transitionSeconds = _slideshowSlides->front().transitionSeconds;
	_transitionTimer = new Timer(transitionSeconds * 1000);
	_transitionTimer->onTimer((StateHandler)&SlideshowState::screenTimer);

	_game->getMod()->playMusic(_slideshowHeader.musicId);
	_game->getCursor()->setVisible(false);
	screenClick(0);
#endif
}

SlideshowState::~SlideshowState()
{
#ifdef __EMSCRIPTEN__
	if (_opened)
	{
		EM_ASM({ if (globalThis.CalypsoSlidePlayer) CalypsoSlidePlayer.close($0); }, this);
		_game->getMod()->stopCutsceneMusic(_ownedMusic);
	}
#endif
	delete _transitionTimer;
}

/**
 * Shows the next screen on a timed basis.
 */
void SlideshowState::screenTimer()
{
	screenClick(0);
}

/**
 * Handle timers.
 */
void SlideshowState::think()
{
	if (_transitionTimer) _transitionTimer->think(this, 0);
}

/**
 * Shows the next screen in the slideshow; pops the state when there are no more slides
 */
void SlideshowState::screenClick(Action *action)
{
#ifdef __EMSCRIPTEN__
	if (_closed) return;
	if (++_curScreen >= static_cast<int>(_slideshowSlides->size())) screenSkip(action);
	else showHdSlide();
#else
	if (_curScreen >= 0)
	{
		_slides[_curScreen]->setVisible(false);
		_captions[_curScreen]->setVisible(false);
	}

	++_curScreen;

	// next screen
	if (_curScreen < (int)_slideshowSlides->size())
	{
		int transitionSeconds = _slideshowHeader.transitionSeconds;
		if (_slideshowSlides->at(_curScreen).transitionSeconds > 0)
			transitionSeconds = _slideshowSlides->at(_curScreen).transitionSeconds;
		_transitionTimer->setInterval(transitionSeconds * 1000);
		_transitionTimer->start();
		setStatePalette(_slides[_curScreen]->getPalette());
		_slides[_curScreen]->setVisible(true);
		_captions[_curScreen]->setVisible(true);
		init();
	}
	else
	{
		screenSkip(action);
	}
#endif
}

/**
 * Skips the slideshow
 */
void SlideshowState::screenSkip(Action *)
{
#ifdef __EMSCRIPTEN__
	if (_closed) return;
	_closed = true;
	EM_ASM({ CalypsoSlidePlayer.close($0); }, this);
#else
	// slideshow is over.  restore the screen scale and pop the state
	_game->getCursor()->setVisible(true);
	CutsceneState::resetDisplay(_wasLetterboxed);
#endif
	_game->popState();
}

#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void calypso_cutscene_action(SlideshowState *owner, int action)
{
	Game *game = getCurrentGame();
	if (game && owner && game->isState(owner)) owner->mediaAction(action);
}

void SlideshowState::mediaAction(int action)
{
	if (_closed) return;
	if (action == 1) screenClick(nullptr);
	else if (action == 2) screenSkip(nullptr);
	else if (!_ownedMusic.empty() && _game->getMod()->getCurrentMusicTrack() == _ownedMusic)
	{
		if (action == 3) Music::pause();
		else if (action == 4) Music::resume();
	}
}

void SlideshowState::init()
{
	State::init();
	if (_opened) return;
	const std::string title = tr("STR_CAL_MEDIA_TITLE"), pause = tr("STR_CAL_MEDIA_PAUSE"),
		resume = tr("STR_CAL_MEDIA_RESUME"), next = tr("STR_CAL_MEDIA_NEXT"), skip = tr("STR_CAL_MEDIA_SKIP");
	const int ready = EM_ASM_INT({
		if (!globalThis.CalypsoSlidePlayer) return 0;
		const owner = $0;
		CalypsoSlidePlayer.open(owner, {title:UTF8ToString($1), pause:UTF8ToString($2),
			resume:UTF8ToString($3), next:UTF8ToString($4), skip:UTF8ToString($5)},
			function(action) { Module._calypso_cutscene_action(owner,action); });
		return 1;
	}, this, title.c_str(), pause.c_str(), resume.c_str(), next.c_str(), skip.c_str());
	if (!ready) Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Cutscene player is unavailable");
	_opened = true;
	if (!_slideshowSlides || _slideshowSlides->empty())
	{
		const std::string message = tr("STR_CAL_MEDIA_UNAVAILABLE");
		EM_ASM({ CalypsoSlidePlayer.error($0, UTF8ToString($1)); }, this, message.c_str());
		return;
	}
	if (!_slideshowHeader.musicId.empty())
	{
		auto *mod = _game->getMod();
		const auto previous = mod->getCurrentMusicTrack();
		mod->playMusic(_slideshowHeader.musicId);
		if (mod->getCurrentMusicTrack() != previous) _ownedMusic = mod->getCurrentMusicTrack();
	}
	screenClick(nullptr);
}

void SlideshowState::showHdSlide()
{
	const auto &slide = _slideshowSlides->at(_curScreen);
	try
	{
		Surface image(1, 1);
		image.loadImage(slide.imagePath);
		auto *source = image.getSurface();
		if (!source) throw Exception("Missing slide " + slide.imagePath);
		std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> rgba(
			SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGBA32, 0), SDL_FreeSurface);
		if (!rgba || rgba->pitch != rgba->w * 4) throw Exception("Cannot decode slide " + slide.imagePath);
		const SDL_Color *palette = image.getPalette();
		const SDL_Color color = palette && slide.color >= 0 && slide.color < 256
			? palette[slide.color] : SDL_Color{255,255,255,255};
		const std::string caption = slide.caption.empty() ? std::string() : std::string(tr(slide.caption));
		const int geometry[] = {slide.x, slide.y, slide.w, slide.h, slide.align, slide.valign};
		const int timing[] = {_curScreen, static_cast<int>(_slideshowSlides->size()), slide.transitionSeconds, _slideshowHeader.transitionSeconds};
		const int rgb = (color.r << 16) | (color.g << 8) | color.b;
		const int shown = EM_ASM_INT({
			try {
				const g = $5 >> 2;
				const t = $6 >> 2;
				CalypsoSlidePlayer.show($0, HEAPU8.subarray($1, $1 + $2*$3*4), $2, $3, {
					caption:UTF8ToString($4), x:HEAP32[g], y:HEAP32[g+1], w:HEAP32[g+2], h:HEAP32[g+3], align:HEAP32[g+4], valign:HEAP32[g+5],
					index:HEAP32[t], count:HEAP32[t+1], seconds:HEAP32[t+2], defaultSeconds:HEAP32[t+3],
					color:'#' + $7.toString(16).padStart(6,'0')
				});
				return 1;
			} catch (e) { console.error(e); return 0; }
		}, this, rgba->pixels, rgba->w, rgba->h, caption.c_str(), geometry, timing, rgb);
		if (!shown) throw Exception("Cannot present slide " + slide.imagePath);
	}
	catch (const std::exception &e)
	{
		Log(LOG_WARNING) << e.what();
		const std::string message = tr("STR_CAL_MEDIA_UNAVAILABLE");
		EM_ASM({ CalypsoSlidePlayer.error($0, UTF8ToString($1)); }, this, message.c_str());
	}
}
#endif

}
