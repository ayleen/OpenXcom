#pragma once
// F01 HD hit mapping (T10 grid, T11 selector): pure coordinate translators.
// All inputs must already share one local logical space; DPR is never
// applied inside these helpers. Not #ifdef-guarded (pure-helper convention).

#include <cmath>
#include <cstddef>
#include <optional>

namespace OpenXcom
{
namespace Calypso
{

struct BaseGridCell
{
	int column = 0;
	int row = 0;
};

inline std::optional<std::size_t> calypsoMiniBaseSlotAt(
	double x, double slotW, double pitch, std::size_t count)
{
	if (!std::isfinite(x) || !std::isfinite(slotW) || !std::isfinite(pitch)
		|| slotW <= 0.0 || pitch <= 0.0 || slotW > pitch || x < 0.0)
	{
		return std::nullopt;
	}
	const long long index = static_cast<long long>(std::floor(x / pitch));
	if (index < 0 || static_cast<unsigned long long>(index) >= count)
	{
		return std::nullopt;
	}
	if (x - index * pitch >= slotW)
	{
		return std::nullopt;
	}
	return static_cast<std::size_t>(index);
}

/// Variable-width base-selector slot (F01 cinematic): the selected position
/// is wide (activeW), every other position is narrow (inactiveW). Positions
/// tile the container exactly; a resized container scales every slot by the
/// same factor, so paint and hit stay on one math source.
struct CalypsoSelectorSlot
{
	int x = 0;
	int w = 0;
};

inline CalypsoSelectorSlot calypsoSelectorSlotRect(
	int containerX, int containerW, int index, int selectedIndex,
	int activeW, int inactiveW, int count = 8)
{
	if (index < 0 || index >= count || count <= 0
		|| activeW <= 0 || inactiveW <= 0 || containerW <= 0)
	{
		return {};
	}
	const double total = static_cast<double>(activeW)
		+ static_cast<double>(count - 1) * static_cast<double>(inactiveW);
	if (!(total > 0.0))
	{
		return {};
	}
	const double scale = static_cast<double>(containerW) / total;
	double cursor = 0.0;
	for (int i = 0; i < index; ++i)
	{
		cursor += (i == selectedIndex ? activeW : inactiveW);
	}
	const double own = (index == selectedIndex ? activeW : inactiveW);
	const int x0 = containerX + static_cast<int>(std::lround(cursor * scale));
	const int x1 = containerX + static_cast<int>(std::lround((cursor + own) * scale));
	return {x0, x1 - x0};
}

/// Hit-test mirror of calypsoSelectorSlotRect: half-open slots, misses
/// (including gaps from rounding) resolve to nullopt so native click and
/// reorder guards reject them exactly like far-right legacy positions.
inline std::optional<std::size_t> calypsoSelectorSlotAt(
	double x, int selectedIndex, int activeW, int inactiveW,
	int containerW, int count = 8)
{
	if (!std::isfinite(x) || count <= 0 || activeW <= 0 || inactiveW <= 0
		|| containerW <= 0 || x < 0.0 || x >= static_cast<double>(containerW))
	{
		return std::nullopt;
	}
	for (int i = 0; i < count; ++i)
	{
		const CalypsoSelectorSlot slot = calypsoSelectorSlotRect(
			0, containerW, i, selectedIndex, activeW, inactiveW, count);
		if (x >= static_cast<double>(slot.x)
			&& x < static_cast<double>(slot.x + slot.w))
		{
			return static_cast<std::size_t>(i);
		}
	}
	return std::nullopt;
}

inline int calypsoBaseDeckEdge(int side, int index)
{
	if (side <= 0)
	{
		return 0;
	}
	if (index <= 0)
	{
		return 0;
	}
	if (index >= 6)
	{
		return side;
	}
	return static_cast<int>(std::floor(static_cast<double>(index) * side / 6.0));
}

struct BaseGridCellRect
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

inline BaseGridCellRect calypsoBaseDeckCellRect(
	int deckX, int deckY, int side, int x, int y, int sizeX, int sizeY)
{
	const int x0 = deckX + calypsoBaseDeckEdge(side, x);
	const int y0 = deckY + calypsoBaseDeckEdge(side, y);
	const int x1 = deckX + calypsoBaseDeckEdge(side, x + sizeX);
	const int y1 = deckY + calypsoBaseDeckEdge(side, y + sizeY);
	return {x0, y0, x1 - x0, y1 - y0};
}

inline std::optional<BaseGridCell> calypsoBaseGridCellAt(
	double x, double y, double width, double height)
{
	if (!std::isfinite(x) || !std::isfinite(y)
		|| !std::isfinite(width) || !std::isfinite(height)
		|| width <= 0.0 || height <= 0.0
		|| x < 0.0 || y < 0.0 || x >= width || y >= height)
	{
		return std::nullopt;
	}
	// Exact inverse of the painted floor edges: compare integer edges against
	// the floored position so hit and paint agree on fractional boundaries.
	const long long xi = static_cast<long long>(std::floor(x));
	const long long yi = static_cast<long long>(std::floor(y));
	const long long sideX = static_cast<long long>(std::floor(width));
	const long long sideY = static_cast<long long>(std::floor(height));
	int column = static_cast<int>(xi * 6 / sideX);
	int row = static_cast<int>(yi * 6 / sideY);
	if (column < 0)
	{
		column = 0;
	}
	if (column > 5)
	{
		column = 5;
	}
	if (row < 0)
	{
		row = 0;
	}
	if (row > 5)
	{
		row = 5;
	}
	while (column + 1 < 6
		&& calypsoBaseDeckEdge(static_cast<int>(sideX), column + 1) <= xi)
	{
		++column;
	}
	while (column > 0 && calypsoBaseDeckEdge(static_cast<int>(sideX), column) > xi)
	{
		--column;
	}
	while (row + 1 < 6
		&& calypsoBaseDeckEdge(static_cast<int>(sideY), row + 1) <= yi)
	{
		++row;
	}
	while (row > 0 && calypsoBaseDeckEdge(static_cast<int>(sideY), row) > yi)
	{
		--row;
	}
	return BaseGridCell{column, row};
}

} // namespace Calypso
} // namespace OpenXcom
