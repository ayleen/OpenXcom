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
/*
 * Phase 46.2-HD.5 (Calypso) -- see CalypsoErrorPopupUi.h.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoErrorPopupUi.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <SDL_ttf.h>

#include "../Engine/FileMap.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Menu/ErrorMessageState.h"
#include "../Mod/Mod.h"

#include "CalypsoF34ErrorLayout.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdTextRasterKey.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoUiMetrics.h"
#include "CalypsoViewportRuntime.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

// Fixed HD theme for the physical replacement only. The legacy bitmap
// fallback (physical unavailable) keeps using the caller-supplied palette
// index (`_window`/`_btnOk`/`_txtMessage`'s existing `color`), unchanged --
// this is a deliberate simplification versus the pilot's per-caller runtime
// theme (STR_CAL_ERROR_CALLER_THEME): one approved dark/mint theme for every
// physically-rendered popup regardless of severity.
constexpr std::uint32_t kWindowBorderRgba    = 0xff74ffb0u;
constexpr std::uint32_t kWindowFillRgba      = 0xff102a24u;
constexpr std::uint32_t kIconPanelBorderRgba = 0xffffc14du;
constexpr std::uint32_t kIconPanelFillRgba   = 0xff1b3f37u;
constexpr std::uint32_t kButtonBorderRgba    = 0xff74ffb0u;
constexpr std::uint32_t kButtonFillRgba      = 0xff164c3du;
constexpr std::uint32_t kGoldTextRgba        = 0xffffc14du; // icon + warning heading
constexpr std::uint32_t kNearWhiteTextRgba   = 0xffe8fff5u; // message + button label
constexpr std::uint32_t kPaleGreenTextRgba   = 0xffcfe9e0u; // detail

CalypsoLayoutClass currentF34LayoutClass()
{
	const CalypsoViewportRuntime& viewport = calypsoViewportRuntime();
	if (viewport.hasLayout()) return viewport.current().layoutClass;
	return calypsoClassifySafeArea(Options::baseXResolution, Options::baseYResolution);
}

void applyRect(Surface* surface, const CalypsoF34Rect& rect)
{
	surface->setX(rect.x);
	surface->setY(rect.y);
	surface->setWidth(rect.width);
	surface->setHeight(rect.height);
}

/// F34 splits a plain error string into a short headline (kept in the
/// existing `_txtMessage`) plus an optional detail line (`_hdMessageDetail`):
/// break at the first '!' or, failing that, the first ". ". Ported from the
/// pilot's splitF34ErrorMessage (CalypsoCommonRecordsStateUi.cpp) verbatim --
/// pure string logic, no engine dependency.
std::pair<std::string, std::string> splitErrorMessage(const std::string& message)
{
	std::size_t split = message.find('!');
	if (split == std::string::npos)
	{
		const std::size_t period = message.find(". ");
		if (period != std::string::npos) split = period;
	}
	if (split == std::string::npos || split + 1 >= message.size())
		return { message, std::string() };

	std::string detail = message.substr(split + 1);
	const std::size_t first = detail.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return { message, std::string() };
	detail.erase(0, first);
	return { message.substr(0, split + 1), detail };
}

CalypsoLogicalRect widgetRect(const Surface* surface)
{
	return { surface->getX(), surface->getY(), surface->getWidth(), surface->getHeight() };
}

/// Greedy word-wrap against the SAME face the physical raster will use
/// (opened at `physicalPixelHeight`), so the wrap decision and the rasterised
/// result always agree -- there is no separate "processed break" export to
/// reuse from the bitmap Text widget (HD.3's rasteriser is a single-line
/// TTF_RenderUTF8_Blended call; embedded '\n's are what make it multi-line).
/// Falls back to the unwrapped string if the face cannot be opened (the
/// caller then submits one, possibly-overflowing line rather than nothing).
std::string wrapForWidth(const std::string& utf8Text, const std::string& vfsPath,
	int physicalPixelHeight, int targetWidthPx, std::vector<int>& breakOffsets)
{
	if (utf8Text.empty() || physicalPixelHeight <= 0) return utf8Text;
	if (!FileMap::fileExists(vfsPath)) return utf8Text;
	SDL_RWops* rw = FileMap::getRWops(vfsPath);
	if (!rw) return utf8Text;
	TTF_Font* face = TTF_OpenFontRW(rw, /*freesrc=*/1, physicalPixelHeight);
	if (!face) return utf8Text;

	std::vector<std::string> words;
	std::string cur;
	for (char c : utf8Text)
	{
		if (c == ' ')
		{
			if (!cur.empty()) { words.push_back(cur); cur.clear(); }
		}
		else cur += c;
	}
	if (!cur.empty()) words.push_back(cur);

	std::string result;
	std::string line;
	for (const std::string& word : words)
	{
		const std::string candidate = line.empty() ? word : line + " " + word;
		int w = 0, h = 0;
		if (!line.empty() && TTF_SizeUTF8(face, candidate.c_str(), &w, &h) == 0 && w > targetWidthPx)
		{
			breakOffsets.push_back(static_cast<int>(result.size() + line.size()));
			result += line;
			result += '\n';
			line = word;
		}
		else
		{
			line = candidate;
		}
	}
	result += line;
	TTF_CloseFont(face);
	return result;
}

/// Draw one beveled panel (a border-colour fill plus an inset fill-colour
/// rect on top) at `widget`'s CURRENT logical rect, then claim `widget` so
/// its own logical draw renders nothing this frame. `widget`'s rect is read
/// live (after State's uniform UI-scaling has positioned it), so this always
/// matches whatever the widget is really occupying on screen.
void submitBeveledPanel(CalypsoHdUiOverlay& overlay, Surface* widget,
	std::uint32_t borderRgba, std::uint32_t fillRgba, int border)
{
	if (!widget) return;
	const CalypsoLogicalRect r = widgetRect(widget);
	if (r.w <= 0 || r.h <= 0) return;
	overlay.submitPanel({ r, borderRgba });
	const CalypsoLogicalRect inner{ r.x + border, r.y + border, r.w - border * 2, r.h - border * 2 };
	if (inner.w > 0 && inner.h > 0) overlay.submitPanel({ inner, fillRgba });
	overlay.claimWidget(widget);
}

/// Submit one HD text label at `widget`'s current logical rect and claim it.
/// `linesHint` is the assumed number of lines the box was authored for (1 for
/// the icon glyph/heading/button label, 2 for the wrapped message/detail
/// boxes); `physicalPixelHeight = logicalHeight * 2 / linesHint` is the
/// simple device-scale default (DPR clamps at 2) -- exact DPR-perfect sizing
/// is a later refinement, not required for this checkpoint.
template <class Widget>
void submitLabel(CalypsoHdUiOverlay& overlay, const CalypsoTtfSourceDescriptor& font,
	Widget* widget, std::uint32_t colorRgba, int linesHint)
{
	if (!widget) return;
	const std::string text = widget->getText();
	if (text.empty()) return;

	const CalypsoLogicalRect rect = widgetRect(widget);
	if (rect.w <= 0 || rect.h <= 0) return;

	const int hint = linesHint > 0 ? linesHint : 1;
	const int physicalPixelHeight = std::max(1, (rect.h * 2) / hint);

	std::vector<int> breakOffsets;
	std::string resolved = text;
	if (hint > 1)
	{
		resolved = wrapForWidth(text, font.canonicalVfsPath, physicalPixelHeight, rect.w * 2, breakOffsets);
	}

	CalypsoHdTextRasterKey key;
	key.source = font;
	key.physicalPixelHeight = physicalPixelHeight;
	key.text = resolved;
	key.breakSignature = calypsoHashLineBreaks(breakOffsets);
	key.colorRgba = colorRgba;
	key.direction = CalypsoTextDirection::LTR;

	overlay.submitText({ key, rect });
	overlay.claimWidget(widget);
}

} // namespace

/// Per-frame feeder: submits the physical replacement for the current
/// popup and claims every logical widget it replaces. Added as a normal
/// state Surface so its draw() runs during the state's own blit, i.e. before
/// Screen::flip()'s post-composite HD UI stage consumes this frame's
/// submissions. A no-op (nothing submitted, nothing claimed) whenever the
/// gate is off, the overlay cannot go physical this frame, or either F34 font
/// fails to resolve -- the complete, unmodified logical popup is always the
/// fallback; there is no persistent "physical-only" flag on any widget.
class CalypsoErrorPopupFeeder final : public Surface
{
public:
	explicit CalypsoErrorPopupFeeder(ErrorMessageState* state) : Surface(1, 1, 0, 0), _state(state) {}

	/// Surface::blit() only calls draw() when its `_redraw` dirty flag is set
	/// (a static, unchanging widget draws once and is thereafter just
	/// re-blitted from its cached pixel buffer). This feeder has no pixel
	/// content of its own and MUST run its submit/claim side effects on every
	/// single state blit (claims and the overlay's pending queues are
	/// frame-scoped and are cleared every frame), so it overrides blit()
	/// directly rather than relying on the dirty-flag-gated base behaviour.
	void blit(SDL_Surface*) override
	{
		draw();
	}

	void draw() override
	{
		Surface::draw();
		if (!_state || !_state->_hdLayout || !_state->_game) return;

		// Non-fullscreen states below the topmost one still blit() every frame
		// (Game::run() walks back to the last full-screen state and draws
		// forward). Only feed the overlay while this popup really is the top
		// state -- otherwise a state pushed on top of it would render UNDER
		// this popup's physical replacement (drawn last, after every state's
		// composite), a z-order bug the plain logical fallback never had.
		if (_state->_game->getTopState() != _state) return;

		CalypsoHdUiOverlay& overlay = CalypsoHdUiOverlay::instance();
		if (!overlay.mayGoPhysical() || !overlay.frozenMetrics().valid()) return;

		Mod* mod = _state->_game->getMod();
		CalypsoTtfSourceDescriptor heading;
		CalypsoTtfSourceDescriptor body;
		if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)) return;
		if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_MONO", body)) return;

		const int border = calypsoBorderFor(_state->_hdWideLayout
			? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

		submitBeveledPanel(overlay, _state->_window, kWindowBorderRgba, kWindowFillRgba, border);
		submitBeveledPanel(overlay, _state->_hdIconPanel, kIconPanelBorderRgba, kIconPanelFillRgba, border);
		submitBeveledPanel(overlay, _state->_btnOk, kButtonBorderRgba, kButtonFillRgba, border);

		submitLabel(overlay, heading, _state->_hdIcon, kGoldTextRgba, 1);
		submitLabel(overlay, body, _state->_hdWarning, kGoldTextRgba, 1);
		submitLabel(overlay, heading, _state->_txtMessage, kNearWhiteTextRgba, 2);
		submitLabel(overlay, body, _state->_hdMessageDetail, kPaleGreenTextRgba, 2);
		submitLabel(overlay, heading, _state->_btnOk, kNearWhiteTextRgba, 1);
	}

private:
	ErrorMessageState* _state;
};

void CalypsoErrorPopupUi::applyRects(ErrorMessageState& state, const CalypsoF34ErrorLayout& layout)
{
	applyRect(state._window, layout.window);
	applyRect(state._hdIconPanel, layout.iconPanel);
	applyRect(state._hdIcon, layout.icon);
	applyRect(state._hdWarning, layout.warning);
	applyRect(state._txtMessage, layout.message);
	applyRect(state._hdMessageDetail, layout.messageDetail);
	applyRect(state._btnOk, layout.acknowledge);
}

void CalypsoErrorPopupUi::configure(ErrorMessageState& state)
{
	state._hdLayout = state._game && state._game->getMod()
		&& state._game->getMod()->isHdUiFamilyEnabled("F34");
	if (!state._hdLayout) return;

	state._hdWideLayout = currentF34LayoutClass() == CalypsoLayoutClass::Wide;
	const CalypsoF34ErrorLayout layout = calypsoF34ErrorLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

	// Extra HD-only widgets. `_hdIconPanel` is a plain (invisible) Surface --
	// pure geometry so its rect can ride State's existing uniform UI-scaling
	// capture alongside the real widgets; the icon badge itself is drawn only
	// as a physical submitPanel (no bitmap-fallback rendering for it).
	state._hdIconPanel = new Surface(1, 1, 0, 0);
	state._hdIcon = new Text(1, 1, 0, 0);
	state._hdWarning = new Text(1, 1, 0, 0);
	state._hdMessageDetail = new Text(1, 1, 0, 0);
	state.add(state._hdIconPanel);
	state.add(state._hdIcon);
	state.add(state._hdWarning);
	state.add(state._hdMessageDetail);
	state._hdIconPanel->setVisible(false);

	CalypsoErrorPopupUi::applyRects(state, layout);

	const Uint8 themeColor = state._txtMessage->getColor();

	state._hdIcon->setSmall();
	state._hdIcon->setColor(themeColor);
	state._hdIcon->setAlign(ALIGN_CENTER);
	state._hdIcon->setVerticalAlign(ALIGN_MIDDLE);
	state._hdIcon->setText("!");

	state._hdWarning->setSmall();
	state._hdWarning->setColor(themeColor);
	state._hdWarning->setAlign(ALIGN_LEFT);
	state._hdWarning->setVerticalAlign(ALIGN_MIDDLE);
	state._hdWarning->setWordWrap(true);
	state._hdWarning->setText(state.tr("STR_CAL_ERROR_OPERATIONAL_WARNING"));

	const std::pair<std::string, std::string> split = splitErrorMessage(state._txtMessage->getText());
	state._txtMessage->setAlign(ALIGN_LEFT);
	state._txtMessage->setVerticalAlign(ALIGN_MIDDLE);
	state._txtMessage->setWordWrap(true);
	state._txtMessage->setText(split.first);

	state._hdMessageDetail->setSmall();
	state._hdMessageDetail->setColor(themeColor);
	state._hdMessageDetail->setAlign(ALIGN_LEFT);
	state._hdMessageDetail->setVerticalAlign(ALIGN_MIDDLE);
	state._hdMessageDetail->setWordWrap(true);
	state._hdMessageDetail->setText(split.second);

	// Capture every widget's design-space rect (set above) and fit/center it
	// into the engine's actual logical canvas. One-shot per state (State's
	// _uiCaptured guard) -- see CalypsoErrorPopupUi::resize().
	state.enableUiScaling(layout.designWidth, layout.designHeight, 1.0f);

	// The feeder's draw() must run BEFORE the widgets it claims/suppresses this
	// frame -- claimWidget() only takes effect for checks that happen AFTER it
	// runs in the same _surfaces draw pass (Text/TextButton/Window all check
	// "claimed this frame?" inside their own draw()). Every real widget above
	// was already add()-ed earlier (either here or in the base create()), so
	// adding the feeder last and then moving it to the FRONT of _surfaces is
	// what actually orders it first -- mirrors the pilot's
	// moveF34PanelsBehindContent reordering trick (CalypsoCommonRecordsStateUi.cpp).
	state._hdFeeder = new CalypsoErrorPopupFeeder(&state);
	state.add(state._hdFeeder);
	auto feederIt = std::find(state._surfaces.begin(), state._surfaces.end(), state._hdFeeder);
	if (feederIt != state._surfaces.end())
	{
		state._surfaces.erase(feederIt);
		state._surfaces.insert(state._surfaces.begin(), state._hdFeeder);
	}
}

bool CalypsoErrorPopupUi::resize(ErrorMessageState& state)
{
	if (!state._hdLayout) return false;
	state.applyUiScaling();
	return true;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
