/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- HD UI overlay queue. Whole-file Emscripten guard.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdUiOverlay.h"
#include "CalypsoViewportMailbox.h"

#include <emscripten.h>

namespace OpenXcom
{
namespace Calypso
{

CalypsoHdUiOverlay& CalypsoHdUiOverlay::instance()
{
	static CalypsoHdUiOverlay s_overlay;
	return s_overlay;
}

void CalypsoHdUiOverlay::beginFrame(int logicalWidth, int logicalHeight)
{
	// Backing-store poll: the canvas width/height are physical device pixels
	// (mirrors the existing Screen::flip() poll). Authoritative for physical
	// canvas dimensions only; the JS observation owns every other field.
	const int physW = (int)EM_ASM_INT({ return document.getElementById('canvas').width; });
	const int physH = (int)EM_ASM_INT({ return document.getElementById('canvas').height; });
	if (physW > 0 && physH > 0)
	{
		calypsoHdViewportBackingStorePoll(physW, physH);
	}

	// Freeze ONE metrics snapshot for the whole frame.
	_frozenMetrics = calypsoHdBuildPresentationMetrics(logicalWidth, logicalHeight);

	// Advance the frame; decide whether HD physical output is permitted.
	const CalypsoHdFrameController::BeginResult r =
		_controller.beginFrame(_frozenMetrics.valid());
	_mayGoPhysical = r.mayGoPhysical;
}

bool CalypsoHdUiOverlay::renderStages()
{
	// Dormant until an adapter submits an enabled group (HD.4+). Nothing to
	// draw and no claims to fail, so the frame presents normally.
	if (!hasEnabledGroups() || !_mayGoPhysical)
	{
		return true;
	}

	// HD.4+ will run the ordered HD UI + diagnostics stages here, returning
	// false (and latching a logical next frame) on a post-claim draw failure.
	return true;
}

void CalypsoHdUiOverlay::contextLost()
{
	_controller.noteContextLost();
	_mayGoPhysical = false;
}

void CalypsoHdUiOverlay::contextRestored()
{
	_controller.noteContextRestored();
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
