#pragma once
// Calypso HD scrollable-collection interaction geometry (pure, natively testable).
//
// Single source of truth for header-aware row hits, quantity stepper targets,
// presentation control-token stripping, select popup layout/hits, and text caret
// paint state shared by the Logistics adapters and the shared collection painter.
// All inputs are engine-logical px; DPR is never applied here. Every helper is
// header-inline and allocation-free except the sanitizer, which returns by value.
#include <cstddef>
#include <string>
#include <vector>
#include <cmath>

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoCollectionRect
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};


/// Native row index for a pointer ordinate, or -1 when the pointer sits in the
/// header band or outside every painted row. headerBottomY is the first painted
/// row's top edge (viewport.y + headerHeight); starting hits there keeps the
/// highlight exactly under the pointer instead of one header above it.
inline int calypsoCollectionRowHitIndex(int pointerY, int headerBottomY, int rowHeight, int rowCount)
{
	if (rowHeight <= 0 || rowCount <= 0)
	{
		return -1;
	}
	const int offset = pointerY - headerBottomY;
	if (offset < 0)
	{
		return -1;
	}
	const int index = offset / rowHeight;
	if (index >= rowCount)
	{
		return -1;
	}
	return index;
}


/// Strip OXCE presentation control tokens from a display string: raw 0x01
/// TOK_COLOR_FLIP bytes (what Language maps {ALT} to at runtime) and literal
/// "{ALT}" markers (what pre-mapping authoring copy carries), then trim outer
/// ASCII whitespace. Byte-oriented: UTF-8 continuation bytes are never split,
/// so currency symbols, NBSP thousand separators, and localized text survive.
inline std::string calypsoStripPresentationControls(const std::string& text)
{
	std::string out;
	out.reserve(text.size());
	for (std::size_t i = 0; i < text.size();)
	{
		const unsigned char c = static_cast<unsigned char>(text[i]);
		if (c == 0x01)
		{
			++i;
			continue;
		}
		if (c == '{' && text.compare(i, 5, "{ALT}") == 0)
		{
			i += 5;
			continue;
		}
		out.push_back(text[i]);
		++i;
	}
	std::size_t first = out.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
	{
		return std::string();
	}
	std::size_t last = out.find_last_not_of(" \t\r\n");
	return out.substr(first, last - first + 1);
}


struct CalypsoCollectionRowSteppers
{
	CalypsoCollectionRect decrement;
	CalypsoCollectionRect increment;
};

/// Deterministic decrement/increment targets inside one authored adjustment
/// cell: decrement owns the leading 44px, increment the trailing 44px, both at
/// full row height. The generator guarantees adjustment cells at least 88px
/// wide (44px policy); narrower cells fail closed here with empty targets
/// instead of overlapping touch zones.
inline CalypsoCollectionRowSteppers calypsoCollectionRowSteppers(const CalypsoCollectionRect& cell)
{
	CalypsoCollectionRowSteppers out;
	if (cell.w < 88 || cell.h < 44)
	{
		return out;
	}
	out.decrement.x = cell.x;
	out.decrement.y = cell.y;
	out.decrement.w = 44;
	out.decrement.h = cell.h;
	out.increment.x = cell.x + cell.w - 44;
	out.increment.y = cell.y;
	out.increment.w = 44;
	out.increment.h = cell.h;
	return out;
}


/// Popup list rect for an expanded select anchored at its closed control rect:
/// opens directly below the anchor and stays bounded within the given
/// dialog/content window, rising above the anchor when there is no room below
/// and shrinking to the window when the option list is taller than it.
/// The window here is the dialog/content window, never the collection
/// viewport: the anchor lives in the control bar above the viewport, so
/// bounding by the viewport could never contain the popup.
inline CalypsoCollectionRect calypsoCollectionPopupListRect(
	const CalypsoCollectionRect& anchor, int optionCount, int optionHeight,
	const CalypsoCollectionRect& window)
{
	CalypsoCollectionRect out;
	if (optionCount <= 0 || optionHeight <= 0 || window.w <= 0 || window.h <= 0)
	{
		return out;
	}
	int height = optionCount * optionHeight;
	if (height > window.h)
	{
		height = window.h;
	}
	int y = anchor.y + anchor.h;
	if (y + height > window.y + window.h)
	{
		y = anchor.y - height;
	}
	if (y < window.y)
	{
		y = window.y;
	}
	out.x = anchor.x;
	out.y = y;
	out.w = anchor.w;
	out.h = height;
	return out;
}

/// Option index for a pointer ordinate inside a popup list rect, or -1 when
/// the pointer is above, below, or past the last live option. Selection and
/// hover stay with the caller: the index maps into the live option vector.
inline int calypsoCollectionPopupOptionHitIndex(
	int pointerY, const CalypsoCollectionRect& popup, int optionHeight, int optionCount)
{
	if (optionHeight <= 0 || optionCount <= 0 || popup.h <= 0)
	{
		return -1;
	}
	const int offset = pointerY - popup.y;
	if (offset < 0)
	{
		return -1;
	}
	const int index = offset / optionHeight;
	if (index >= optionCount)
	{
		return -1;
	}
	return index;
}


/// Caret paint gate: visible only while the input owns focus and the blink
/// phase is on. The native widget stays the behavior owner; HD paints the
/// caret instead of the suppressed native glyph.
inline bool calypsoCollectionTextCaretVisible(bool focused, bool blinkOn)
{
	return focused && blinkOn;
}

/// Caret x offset for a native caret position over per-code-point advances.
/// UTF-8-safe: advances are per code point, bytes are never indexed.
/// Out-of-range positions clamp to the text end; empty metrics yield 0.
inline int calypsoCollectionTextCaretX(const std::vector<int>& advances, std::size_t caretIndex)
{
	int x = 0;
	const std::size_t end = caretIndex < advances.size() ? caretIndex : advances.size();
	for (std::size_t i = 0; i < end; ++i)
	{
		x += advances[i];
	}
	return x;
}

/// Map a font-pixel advance (caretAdvance output plus measured prefix) into
/// the painted row-text coordinate scale. Glyphs rasterize at rowPx from a
/// fontPixelSize face, so the factor is rowPx/fontPixelSize — never the modal
/// motion ramp. Invalid sizes fail closed at the text origin; the caller
/// keeps its field-right clamp only as a backstop. Allocation-free.
inline int calypsoCollectionCaretProjectedX(int textX, double fontAdvancePx, int rowPx, int fontPixelSize)
{
	if (fontPixelSize <= 0 || rowPx <= 0)
	{
		return textX;
	}
	return textX + (int)std::llround(fontAdvancePx * ((double)rowPx / (double)fontPixelSize));
}

} // namespace Calypso
} // namespace OpenXcom
