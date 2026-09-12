#ifdef __EMSCRIPTEN__

#include "../Interface/TextList.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../Engine/Action.h"
#include "../Engine/Font.h"
#include "../Engine/TTFFont.h"
#include "../fmath.h"
#include "../Interface/ArrowButton.h"
#include "../Interface/ScrollBar.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSelectionListScroll.h"

namespace OpenXcom
{

void TextList::rebaseNativeSize(int nativeW, int nativeH)
{
	// Re-anchor the HD scale() denominator to the current design so authored
	// design-space column widths are not re-scaled by the legacy 280 native width
	// (external review #2). Only meaningful before rows are (re)built.
	if (nativeW > 0) _nativeW = nativeW;
	if (nativeH > 0) _nativeH = nativeH;
}

bool TextList::calypsoHdUpdateVisibleFastPath()
{
	if (_hdSelList && _hdRowStride > 0 && _hdVisibleRows > 0)
	{
		_visibleRows = _hdVisibleRows;
		if (!_rows.empty() && _scroll + _visibleRows > _rows.size())
		{
			_scroll = _rows.size() > _visibleRows ? _rows.size() - _visibleRows : 0;
		}
		else if (_rows.size() <= _visibleRows)
		{
			_scroll = 0;
		}
		updateArrows();
		return true;
	}
	return false;
}

bool TextList::calypsoHdHandleResizedHeight()
{
	if (_hdSelList)
	{
		updateVisible();
		positionCalypsoHdScrollbar();
		return true;
	}
	return false;
}

bool TextList::calypsoHdClaimedThisFrame() const
{
	// Phase 46.2-HD (A2): when the HD overlay has claimed this list this frame, the
	// physical HD rows replace the logical ROW TEXT (drawn post-composite). But the
	// selector highlight, toggle arrows, and scrollbar are NOT HD-replaced, so we
	// must keep drawing them -- suppressing the whole blit erased all selection /
	// scroll-position feedback (external review #8). Input/scroll handling is
	// unaffected either way; the row-text cache is left intact for an unclaimed
	// later frame.
	return Calypso::CalypsoHdUiOverlay::instance().widgetClaimed(this,
		Calypso::CalypsoHdUiOverlay::instance().frameId());
}

bool TextList::calypsoHdRoutePointerToScrollbar(Action *action, State *state)
{
	// HD ownership at entry: track/drag pointer input belongs to the ScrollBar
	// exclusively and must never arm the TextList (else a drag released over a
	// row activates a facility via InteractiveSurface::mouseClick).
	if (_hdSelList && _scrollbar && (action->getDetails()->type == SDL_MOUSEBUTTONDOWN ||
		action->getDetails()->type == SDL_MOUSEBUTTONUP || action->getDetails()->type == SDL_MOUSEMOTION))
	{
		if (_scrollbar->calypsoHdIsDragging())
		{
			_scrollbar->handle(action, state);
			return true;
		}
		if (action->getDetails()->type != SDL_MOUSEBUTTONUP &&
			isCalypsoHdTrackHit(action->getAbsoluteXMouse(), action->getAbsoluteYMouse()))
		{
			_scrollbar->handle(action, state);
			return true;
		}
	}
	return false;
}

bool TextList::calypsoHdSuppressClick(Action *action) const
{
	// Track clicks follow normal scrollbar behavior; they must never select
	// or activate a facility row. Middle-click ufopaedia actions are untouched.
	return _hdSelList && action->getDetails()->button.button == SDL_BUTTON_LEFT &&
		isCalypsoHdTrackHit(action->getAbsoluteXMouse(), action->getAbsoluteYMouse());
}

bool TextList::calypsoHdFilterMouseOver(Action *action, State *state)
{
	// A drag crossing row content must not reselect rows mid-gesture.
	if (_hdSelList && _scrollbar->calypsoHdIsDragging())
	{
		InteractiveSurface::mouseOver(action, state);
		return true;
	}
	// A stationary synthetic hover refresh must not overwrite keyboard
	// selection; real pointer moves/clicks still select rows normally.
	if (_hdSelList)
	{
		const double absX = action->getAbsoluteXMouse();
		const double absY = action->getAbsoluteYMouse();
		if (absX == _hdLastHoverX && absY == _hdLastHoverY)
		{
			SDL_Event *hdEv = action->getDetails();
			const bool hdRealClick = (hdEv && hdEv->type == SDL_MOUSEBUTTONDOWN
				&& (hdEv->button.button == SDL_BUTTON_LEFT || hdEv->button.button == SDL_BUTTON_MIDDLE || hdEv->button.button == SDL_BUTTON_RIGHT));
			if (!hdRealClick)
			{
				InteractiveSurface::mouseOver(action, state);
				return true;
			}
		}
		_hdLastHoverX = absX;
		_hdLastHoverY = absY;
	}
	return false;
}

Calypso::CalypsoSelectionListRowHit TextList::calypsoHdRowHit(const Action *action) const
{
	if (!_hdSelList || _hdRowStride <= 0 || _hdVisibleRows == 0)
	{
		return {};
	}
	// Engine-logical pointer position: the same space the adapter projected the
	// row slots into. getRelativeYMouse() would be display px — one conversion,
	// never mixed systems.
	const double relativeLogicalY = action->getAbsoluteYMouse() - static_cast<double>(getY());
	return Calypso::calypsoSelectionListRowAtLogicalY(
		relativeLogicalY, static_cast<double>(_hdRowStride),
		static_cast<double>(_hdRowOriginY), _scroll, _rows.size(), _hdVisibleRows);
}

void TextList::calypsoHdMaybeApplyTtf(Text *txt)
{
	// Calypso: give rows added after setTTFFont (Options lists populate in init(),
	// after applyTTFToTexts) the same crisp HD text — else they render as small
	// native bitmap glyphs inside the scaled (tall) row box.
	if (_ttfFont)
	{
		txt->setTTFFont(_ttfFont, _ttfFrac);
	}
}

void TextList::calypsoHdNormalizeRowHeights(std::vector<Text*> &row, int rowHeight, int cols)
{
	// Calypso: the row Text boxes were created at scaled height, but rowHeight is
	// measured from the (unscaled) bitmap font — so scale it back up. Without this
	// the box collapses to native height and the HD TTF glyphs get downscaled to a
	// tiny block at the top of a tall, mostly-empty row (this is what made combobox
	// dropdowns + the Advanced/Controls option lists render as small text). The
	// stride math in draw()/blit()/updateVisible already reads getHeight() and
	// scales _font metrics by scale(), so a scaled box keeps everything aligned.
	const int scaledRowHeight = std::max((int)Round(rowHeight * scale()),
		(int)Round(_minimumRowHeight * scale()));
	for (int i = 0; i < cols; ++i)
	{
		row[i]->setHeight(scaledRowHeight);
	}
}

/**
 * Calypso: HD — resize the scroll arrows AND scrollbar at the scaled size, then
 * re-run setY to reposition them.
 */
void TextList::setWidth(int w)
{
	// Same-size updates must not recreate the arrows/scrollbar for configured
	// HD lists: recreation drops an in-progress scrollbar drag. Real resizes
	// reset the capture. Ordinary lists keep the legacy path below.
	if (_hdSelList && w == getWidth()) return;
	Surface::setWidth(w);
	// Recreate scroll arrows at scaled size.
	float s = scale();
	int aw = (int)Round(13 * s);
	int ah = (int)Round(14 * s);
	SDL_Color pal[256];
	std::copy(getPalette(), getPalette() + 256, pal);
	Uint8 arrowColor = _up->getColor();
	Uint8 barColor = _scrollbar->getColor();
	delete _up;
	delete _down;
	delete _scrollbar;
	_up = new ArrowButton(ARROW_BIG_UP, aw, ah, getX() + w + _scrollPos, getY());
	_up->setVisible(false);
	_up->setTextList(this);
	_up->setPalette(pal);
	_up->setColor(arrowColor);
	_down = new ArrowButton(ARROW_BIG_DOWN, aw, ah, getX() + w + _scrollPos, getY() + getHeight() - ah);
	_down->setVisible(false);
	_down->setTextList(this);
	_down->setPalette(pal);
	_down->setColor(arrowColor);
	// Recreate the scrollbar at the scaled arrow width and the post-scale X. The
	// base ctor sized it to the native arrow width and applyUiScaling's setX ran
	// while getWidth() was still native, so it was left as a thin, mispositioned
	// bar floating over the list (drawn twice — once direct, once via the
	// updateArrows blit onto the now-wide list surface that no longer clips it).
	int sbh = std::max(_down->getY() - _up->getY() - _up->getHeight(), 1);
	_scrollbar = new ScrollBar(aw, sbh, getX() + w + _scrollPos, _up->getY() + _up->getHeight());
	_scrollbar->setVisible(false);
	_scrollbar->setTextList(this);
	_scrollbar->setPalette(pal);
	_scrollbar->setColor(barColor);
	if (_bg) _scrollbar->setBackground(_bg);
	_scrollbar->setHighContrast(_contrast);
	// Re-apply the HD seam onto the recreated scrollbar, then reposition.
	if (_hdSelList)
	{
		_scrollbar->setCalypsoHdMinThumb(_hdMinThumb);
	}
	// Reposition _down and recompute scrollbar height.
	setY(getY());
	setHeight(getHeight());
}

/**
 * Calypso: HD — forward TTF font opt-in to every text cell.
 */
void TextList::setTTFFont(TTFFont* font, float fillFrac)
{
	// Remember it so rows added LATER (Options lists are populated in the state's init(),
	// after OptionsBaseState::init() runs applyTTFToTexts) also render crisp HD text.
	_ttfFont = font;
	_ttfFrac = fillFrac;
	for (auto& row : _texts)
		for (auto* t : row)
			t->setTTFFont(font, fillFrac);
}

void TextList::configureCalypsoHdSelectionList(int scrollBarWidth, int minThumbHeight,
	int rowStride, int rowOriginY, int dataViewportH, size_t visibleRows)
{
	const int cachedStride = rowStride > 0 ? rowStride : 0;
	const int cachedOrigin = rowOriginY > 0 ? rowOriginY : 0;
	const int cachedViewportH = dataViewportH > 0 ? dataViewportH : 0;
	const size_t cachedVisible = visibleRows;
	const bool changed = !_hdSelList || _hdScrollBarWidth != scrollBarWidth || _hdMinThumb != minThumbHeight ||
		_hdRowStride != cachedStride || _hdRowOriginY != cachedOrigin ||
		_hdDataViewportH != cachedViewportH || _hdVisibleRows != cachedVisible;
	_hdSelList = true;
	_hdScrollBarWidth = scrollBarWidth > 0 ? scrollBarWidth : 0;
	_hdMinThumb = minThumbHeight > 0 ? minThumbHeight : 0;
	_hdRowStride = cachedStride;
	_hdRowOriginY = cachedOrigin;
	_hdDataViewportH = cachedViewportH;
	_hdVisibleRows = cachedVisible;
	if (changed)
	{
		_scrollbar->setCalypsoHdMinThumb(_hdMinThumb);
		_scrollbar->calypsoHdCancelDrag();
	}
	else
	{
		_scrollbar->setCalypsoHdMinThumb(_hdMinThumb);
	}
	updateVisible();
	positionCalypsoHdScrollbar();
}

void TextList::clearCalypsoHdSelectionList()
{
	if (!_hdSelList) return;
	_hdSelList = false;
	_hdScrollBarWidth = 0;
	_hdMinThumb = 0;
	_hdRowStride = 0;
	_hdRowOriginY = 0;
	_hdDataViewportH = 0;
	_hdVisibleRows = 0;
	_hdLastHoverX = 1e30;
	_hdLastHoverY = 1e30;
	_scrollbar->clearCalypsoHd();
}

bool TextList::isCalypsoHdSelectionList() const
{
	return _hdSelList;
}

SDL_Rect TextList::getCalypsoHdTrackRect() const
{
	SDL_Rect track;
	track.x = getX();
	track.y = getY();
	track.w = 0;
	track.h = 0;
	if (!_hdSelList || _hdScrollBarWidth <= 0 || getWidth() <= 0 || getHeight() <= 0) return track;
	// The visual track and the native input rail share one geometry: the data
	// viewport (first painted row slot through the last), never the full list
	// rect including the column header or the trailing empty remainder.
	const int originY = _hdRowOriginY > 0 ? _hdRowOriginY : 0;
	int viewportH = _hdDataViewportH > 0 ? _hdDataViewportH : getHeight() - originY;
	if (originY + viewportH > getHeight()) viewportH = getHeight() - originY;
	if (viewportH <= 0) return track;
	const Calypso::CalypsoSelectionListTrack t = Calypso::calypsoSelectionListTrackForList(
		getX(), getY() + originY, getWidth(), viewportH, _hdScrollBarWidth);
	track.x = t.x;
	track.y = t.y;
	track.w = t.w;
	track.h = t.h;
	return track;
}

SDL_Rect TextList::getCalypsoHdThumbRect() const
{
	SDL_Rect thumb;
	thumb.x = getX();
	thumb.y = getY();
	thumb.w = 0;
	thumb.h = 0;
	if (!_hdSelList) return thumb;
	const SDL_Rect track = getCalypsoHdTrackRect();
	if (track.w <= 0 || track.h <= 0) return thumb;
	const std::size_t total = _rows.size();
	const std::size_t visible = _visibleRows;
	const std::size_t maxScroll = Calypso::calypsoSelectionListMaxScroll(total, visible);
	if (maxScroll == 0) return thumb;
	thumb.x = track.x;
	thumb.w = track.w;
	thumb.h = Calypso::calypsoSelectionListThumbHeight(track.h, total, visible, _hdMinThumb);
	thumb.y = track.y + Calypso::calypsoSelectionListThumbOffset(track.h, thumb.h, _scroll, maxScroll);
	return thumb;
}

void TextList::positionCalypsoHdScrollbar()
{
	if (!_hdSelList) return;
	const SDL_Rect track = getCalypsoHdTrackRect();
	if (track.w <= 0 || track.h <= 0) return;
	const bool moved = _scrollbar->getX() != track.x || _scrollbar->getY() != track.y ||
		_scrollbar->getWidth() != track.w || _scrollbar->getHeight() != track.h;
	if (!moved) return;
	// A real geometry change invalidates an in-progress drag grab offset.
	_scrollbar->calypsoHdCancelDrag();
	_scrollbar->setX(track.x);
	_scrollbar->setY(track.y);
	if (_scrollbar->getWidth() != track.w)
		_scrollbar->setWidth(track.w);
	if (_scrollbar->getHeight() != track.h)
		_scrollbar->setHeight(track.h);
}

bool TextList::isCalypsoHdTrackHit(double absX, double absY) const
{
	if (!_hdSelList) return false;
	const SDL_Rect track = getCalypsoHdTrackRect();
	if (track.w <= 0 || track.h <= 0) return false;
	return absX >= track.x && absX < track.x + track.w &&
		absY >= track.y && absY < track.y + track.h;
}

void TextList::calypsoHdMoveSelection(int delta)
{
	if (!_selectable || _texts.empty() || _rows.empty() || delta == 0) return;
	// Current logical row under the native selection index.
	const std::size_t current = static_cast<std::size_t>(getSelectedRow());
	std::size_t target;
	if (current >= _texts.size())
	{
		target = delta > 0 ? 0 : _texts.size() - 1;
	}
	else if (delta > 0)
	{
		target = std::min<std::size_t>(_texts.size() - 1, current + 1);
	}
	else
	{
		target = current > 0 ? current - 1 : 0;
	}
	// setSelectedRow reveals the row (scrolls) and clamps at the endpoints;
	// it never activates handlers.
	setSelectedRow(target);
}

}

#endif
