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

void State::enableUiScaling(int designW, int designH, float factor)
{
	enableUiScaling(designW, designH, factor, false);
}

void State::enableUiScaling(int designW, int designH, float factor, bool geometryIsDesignSpace)
{
	if (_uiCaptured || designW <= 0 || designH <= 0) return;
	_uiDesignW = designW;
	_uiDesignH = designH;
	_uiFactor = factor > 0.0f ? factor : 1.0f;
	_uiNative.clear();
	const int dx = geometryIsDesignSpace ? 0 : _game->getScreen()->getDX();
	const int dy = geometryIsDesignSpace ? 0 : _game->getScreen()->getDY();
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
