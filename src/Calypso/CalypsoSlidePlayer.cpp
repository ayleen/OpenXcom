#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "CalypsoHdUiOverlay.h"
#include "../Engine/Exception.h"
#include "../Engine/FileMap.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Logger.h"
#include "../Engine/Music.h"
#include "../Engine/Screen.h"
#include "../Engine/Surface.h"
#include "../Menu/CutsceneState.h"
#include "../Menu/SlideshowState.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{
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

void Mod::stopCutsceneMusic(const std::string &track)
{
	if (track.empty() || track != _currentMusicTrack) return;
	Music::stop();
	_playingMusic.clear();
	_currentMusicTrack.clear();
}
}
#endif
