#ifdef __EMSCRIPTEN__

#include "../Interface/ScrollBar.h"

#include <cstddef>

#include "../Engine/Action.h"
#include "../Interface/TextList.h"
#include "CalypsoSelectionListScroll.h"

namespace OpenXcom
{

bool ScrollBar::calypsoHdDragScrolled(Action *action, int cursorY)
{
	if (!_hdEnabled) return false;
	const bool hdDragEvent = action->getDetails()->type == SDL_MOUSEMOTION ||
		(action->getDetails()->type == SDL_MOUSEBUTTONDOWN && action->getDetails()->button.button == SDL_BUTTON_LEFT);
	// A wheel tick while held must keep its own scroll step; remapping
	// it from drag coordinates would overwrite it from the cursor.
	if (!hdDragEvent) return true;
	const std::size_t total = _list->getRowsDoNotUse();
	const std::size_t visible = _list->getVisibleRows();
	const std::size_t maxScroll = Calypso::calypsoSelectionListMaxScroll(total, visible);
	if (maxScroll == 0) return true;
	const int thumbH = Calypso::calypsoSelectionListThumbHeight(getHeight(), total, visible, _hdMinThumb);
	// _offset preserves the initial grab position inside the thumb.
	const int thumbOffset = cursorY + _offset;
	_list->scrollTo(Calypso::calypsoSelectionListScrollForOffset(thumbOffset, getHeight(), thumbH, maxScroll));
	return true;
}

bool ScrollBar::calypsoHdPressAt(int cursorY)
{
	if (!_hdEnabled || !_list) return false;
	const SDL_Rect thumb = calypsoHdThumbRect();
	if (cursorY >= thumb.y && thumb.h > 0 && cursorY < thumb.y + thumb.h)
	{
		_offset = thumb.y - cursorY;
	}
	else
	{
		_offset = -thumb.h / 2;
	}
	_pressed = true;
	return true;
}

bool ScrollBar::calypsoHdSyncThumbRect()
{
	if (!_hdEnabled || !_list) return false;
	const SDL_Rect thumb = calypsoHdThumbRect();
	_thumbRect.x = 0;
	_thumbRect.y = thumb.y;
	_thumbRect.w = _thumb->getWidth();
	_thumbRect.h = thumb.h;
	return true;
}

void ScrollBar::setCalypsoHdMinThumb(int minThumb)
{
	if (!_hdEnabled || _hdMinThumb != minThumb)
	{
		_pressed = false;
		_offset = 0;
	}
	_hdEnabled = true;
	_hdMinThumb = minThumb;
	_redraw = true;
}

void ScrollBar::clearCalypsoHd()
{
	if (!_hdEnabled) return;
	_hdEnabled = false;
	_hdMinThumb = 0;
	_pressed = false;
	_offset = 0;
	_redraw = true;
}

bool ScrollBar::calypsoHdIsDragging() const
{
	return _hdEnabled && _pressed;
}

void ScrollBar::calypsoHdCancelDrag()
{
	_pressed = false;
	_offset = 0;
}

SDL_Rect ScrollBar::calypsoHdThumbRect()
{
	SDL_Rect thumb;
	thumb.x = 0;
	thumb.y = 0;
	thumb.w = getWidth();
	thumb.h = 0;
	if (!_list || !_hdEnabled || getHeight() <= 0) return thumb;
	const std::size_t total = _list->getRowsDoNotUse();
	const std::size_t visible = _list->getVisibleRows();
	const std::size_t maxScroll = Calypso::calypsoSelectionListMaxScroll(total, visible);
	if (maxScroll == 0) return thumb;
	thumb.h = Calypso::calypsoSelectionListThumbHeight(getHeight(), total, visible, _hdMinThumb);
	thumb.y = Calypso::calypsoSelectionListThumbOffset(getHeight(), thumb.h, _list->getScroll(), maxScroll);
	return thumb;
}

}

#endif
