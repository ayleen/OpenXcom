#ifdef __EMSCRIPTEN__
/* Browser-only State UI scaling, extracted from Engine/State.cpp. */
#include "../Engine/State.h"

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Screen.h"
#include "../Engine/Surface.h"
#include "../Engine/TTFFont.h"
#include "../Interface/ComboBox.h"
#include "../Interface/Slider.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "CalypsoUiMetrics.h"
#include "CalypsoViewportRuntime.h"

namespace OpenXcom
{

void State::enableUiScaling(int designW, int designH, float factor,
	bool subtractVanillaCenter)
{
	if (_uiCaptured || designW <= 0 || designH <= 0) return;
	_uiDesignW = designW;
	_uiDesignH = designH;
	_uiFactor = factor > 0.0f ? factor : 1.0f;
	_uiNative.clear();
	// Vanilla widgets are created in the 320x200 space and centered into the
	// base canvas (centerAllSurfaces), so capturing their design-space rect
	// requires undoing that centering. Calypso design-space widgets (F33/F34)
	// already carry canonical layout rects and must NOT be shifted (F33: the
	// dialog rendered 105px left / 40px up when dx/dy were subtracted).
	const int dx = subtractVanillaCenter ? _game->getScreen()->getDX() : 0;
	const int dy = subtractVanillaCenter ? _game->getScreen()->getDY() : 0;
	for (auto* surf : _surfaces)
	{
		_uiNative.push_back({ surf, surf->getX() - dx, surf->getY() - dy,
		                      surf->getWidth(), surf->getHeight() });
	}
	_uiCaptured = true;
	applyUiScaling();
}

void State::applyUiScaling()
{
	if (!_uiCaptured) return;
	Calypso::CalypsoBaseSafeRect safe{0, 0, Options::baseXResolution, Options::baseYResolution};
	(void)Calypso::calypsoProjectedSafeRectForLayout(
		Options::baseXResolution, Options::baseYResolution, safe);
	const float s = static_cast<float>(Calypso::calypsoFitUiScale(
		safe, _uiDesignW, _uiDesignH, _uiFactor));
	_uiScale = s;
	const int offX = safe.x + (safe.width - static_cast<int>(_uiDesignW * s + 0.5f)) / 2;
	const int offY = safe.y + (safe.height - static_cast<int>(_uiDesignH * s + 0.5f)) / 2;
	for (const auto& r : _uiNative)
	{
		if (!r.surf) continue;
		const int left = static_cast<int>(r.x * s + 0.5f);
		const int top = static_cast<int>(r.y * s + 0.5f);
		const int right = static_cast<int>((r.x + r.w) * s + 0.5f);
		const int bottom = static_cast<int>((r.y + r.h) * s + 0.5f);
		r.surf->setX(offX + left);
		r.surf->setY(offY + top);
		int w = right - left; if (w < 1) w = 1;
		int h = bottom - top; if (h < 1) h = 1;
		r.surf->setWidth(w);
		r.surf->setHeight(h);
	}
}

void State::recaptureUiScaling(int designW, int designH, float factor,
	bool subtractVanillaCenter)
{
	// Force a fresh capture against the new design canvas. enableUiScaling is
	// one-shot (returns early once _uiCaptured), which is correct for the common
	// case but wrong for a state that swaps its entire layout at runtime: without
	// this the resize path replays the STALE first snapshot and reverts the new
	// rects (external review #3). Clearing the latch and re-running reuses the
	// exact same capture + apply path, so it stays consistent with configure().
	_uiCaptured = false;
	enableUiScaling(designW, designH, factor, subtractVanillaCenter);
}

void State::excludeFromUiScaling(Surface* surf)
{
	for (auto it = _uiNative.begin(); it != _uiNative.end(); ++it)
	{
		if (it->surf == surf) { _uiNative.erase(it); break; }
	}
}

void State::applyTTFToTexts(TTFFont* font, float fillFrac)
{
	if (!font) return;
	for (auto* surf : _surfaces)
	{
		if (auto* t = dynamic_cast<Text*>(surf)) t->setTTFFont(font, fillFrac);
		else if (auto* b = dynamic_cast<TextButton*>(surf)) b->setTTFFont(font, fillFrac);
		else if (auto* sl = dynamic_cast<Slider*>(surf)) sl->setTTFFont(font, fillFrac);
		else if (auto* cb = dynamic_cast<ComboBox*>(surf)) cb->setTTFFont(font, fillFrac);
		else if (auto* tl = dynamic_cast<TextList*>(surf)) tl->setTTFFont(font, fillFrac);
	}
}

} // namespace OpenXcom
#endif /* __EMSCRIPTEN__ */
