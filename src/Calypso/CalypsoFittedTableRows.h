#pragma once
#ifdef __EMSCRIPTEN__
#include "../Interface/TextList.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace OpenXcom { namespace Calypso {
/// The painter scales its design rows to the live window. TextList resolves
/// pointer coordinates in logical pixels, so install the same boundaries
/// after the state has fitted its widgets to the current viewport.
inline void calypsoFitTableHitRows(TextList &list, int designWidth, int rowCount,
	int barWidth, int minThumbHeight)
{
	if (designWidth <= 0 || rowCount <= 0) return;
	const double scale = double(list.getWidth()) / designWidth;
	const double stride = double(list.getHeight()) / rowCount;
	std::vector<int> edges;
	edges.reserve(rowCount + 1);
	for (int i = 0; i <= rowCount; ++i)
		edges.push_back(static_cast<int>(std::lround(i * stride)));
	list.configureCalypsoHdSelectionList(std::max(1, int(std::lround(barWidth * scale))),
		std::max(1, int(std::lround(minThumbHeight * scale))), std::max(1, int(std::lround(stride))),
		0, list.getHeight(), rowCount, edges);
}
} }
#endif
