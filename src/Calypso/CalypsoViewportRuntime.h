#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.1 (Calypso) -- pure viewport runtime holder.
 *
 * Owns the logical safe-area metrics together with the physical canvas size.
 * It has no SDL, JavaScript, browser, engine, or allocation dependency. The
 * Emscripten bridge owns one instance; native doctests exercise the real code.
 */
#include "CalypsoUiMetrics.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoViewportUpdate
{
	bool logicalChanged = false;
	bool physicalChanged = false;
	bool hadPreviousLayout = false;
	int previousLogicalWidth = 0;
	int previousLogicalHeight = 0;
	int previousPhysicalWidth = 0;
	int previousPhysicalHeight = 0;
	std::uint64_t previousGeneration = 0;
	std::uint64_t generation = 0;
	CalypsoLayoutMetrics previousMetrics;
	CalypsoLayoutMetrics metrics;

	bool anyChanged() const { return logicalChanged || physicalChanged; }
};

class CalypsoViewportRuntime
{
public:
	CalypsoViewportUpdate update(int logicalWidth, int logicalHeight,
	                              int physicalWidth, int physicalHeight,
	                              const CalypsoSafeInsets& insets,
	                              CalypsoVisualContext context)
	{
		CalypsoViewportUpdate result;
		result.hadPreviousLayout = _metrics.hasLayout();
		if (result.hadPreviousLayout) result.previousMetrics = _metrics.current();
		result.previousLogicalWidth = _metrics.hasLayout() ? _metrics.current().logicalWidth : 0;
		result.previousLogicalHeight = _metrics.hasLayout() ? _metrics.current().logicalHeight : 0;
		result.previousPhysicalWidth = _physicalWidth;
		result.previousPhysicalHeight = _physicalHeight;
		result.previousGeneration = _metrics.generation();

		result.logicalChanged = _metrics.recompute(logicalWidth, logicalHeight, insets, context);
		result.metrics = _metrics.current();
		result.generation = _metrics.generation();
		const int nextPhysicalWidth = calypsoClampNonneg(physicalWidth);
		const int nextPhysicalHeight = calypsoClampNonneg(physicalHeight);
		result.physicalChanged = !_hasPhysical
		                      || nextPhysicalWidth != _physicalWidth
		                      || nextPhysicalHeight != _physicalHeight;
		_physicalWidth = nextPhysicalWidth;
		_physicalHeight = nextPhysicalHeight;
		_hasPhysical = true;
		return result;
	}

	const CalypsoLayoutMetrics& current() const { return _metrics.current(); }
	const CalypsoViewportMetrics& metrics() const { return _metrics; }
	std::uint64_t generation() const { return _metrics.generation(); }
	bool hasLayout() const { return _metrics.hasLayout(); }
	bool hasPhysicalSize() const { return _hasPhysical; }
	int physicalWidth() const { return _physicalWidth; }
	int physicalHeight() const { return _physicalHeight; }

private:
	CalypsoViewportMetrics _metrics;
	int _physicalWidth = 0;
	int _physicalHeight = 0;
	bool _hasPhysical = false;
};

#ifdef __EMSCRIPTEN__
/// POD copied from the bridge into Game's event transaction. Keeping the SDL
/// event itself dimension-only lets real browser SDL resize probes be rejected.
struct CalypsoPendingViewportResize
{
	bool logicalChanged = false;
	bool physicalChanged = false;
	bool hadPreviousLayout = false;
	int previousLogicalWidth = 0;
	int previousLogicalHeight = 0;
	int logicalWidth = 0;
	int logicalHeight = 0;
	int previousPhysicalWidth = 0;
	int previousPhysicalHeight = 0;
	int physicalWidth = 0;
	int physicalHeight = 0;
	std::uint64_t generation = 0;
	CalypsoLayoutMetrics previousMetrics;
	CalypsoLayoutMetrics metrics;
};

CalypsoViewportRuntime& calypsoViewportRuntime();
bool calypsoConsumePendingViewportResize(int physicalWidth, int physicalHeight,
	                                     CalypsoPendingViewportResize& out);
bool calypsoNotifyCanvasFallback(int physicalWidth, int physicalHeight);
bool calypsoProjectedSafeRectForLayout(int baseWidth, int baseHeight,
	                                   CalypsoBaseSafeRect& out);
#endif

} // namespace Calypso
} // namespace OpenXcom
