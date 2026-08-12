/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- Emscripten bridge/poll adapter around the
 * portable CalypsoViewportModel. Whole-file Emscripten guard per the Phase 36
 * placement policy; the model itself is portable and natively tested.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoViewportMailbox.h"

#include <emscripten.h>

namespace OpenXcom
{
namespace Calypso
{

namespace
{
/// The single bridge-owned HD viewport model.
CalypsoViewportModel s_hdViewportModel;

CalypsoOrientation orientationFromCode(int code)
{
	switch (code)
	{
	case CALYPSO_VP_ORIENT_PORTRAIT:  return CalypsoOrientation::Portrait;
	case CALYPSO_VP_ORIENT_LANDSCAPE: return CalypsoOrientation::Landscape;
	default:                          return CalypsoOrientation::Unknown;
	}
}
} // namespace

CalypsoViewportModel& calypsoHdViewportModel()
{
	return s_hdViewportModel;
}

bool calypsoHdViewportBackingStorePoll(int physicalWidth, int physicalHeight)
{
	return s_hdViewportModel.applyBackingStorePoll(physicalWidth, physicalHeight);
}

CalypsoHdPresentationMetrics calypsoHdBuildPresentationMetrics(
	int logicalWidth, int logicalHeight)
{
	const CalypsoViewportState& s = s_hdViewportModel.state();
	return calypsoMakeStretchMetrics(
		logicalWidth, logicalHeight,
		s.physicalWidth, s.physicalHeight,
		s.dpr, s_hdViewportModel.generation());
}

} // namespace Calypso
} // namespace OpenXcom

// --- JS bridge C ABI -------------------------------------------------------

extern "C"
{

/**
 * Deliver a viewport observation from web/src/main.js. Authoritative for every
 * field whose validity bit (CalypsoViewportValidBit) is set in `validMask`.
 * `revision` is monotonic per JS runtime; the model rejects stale/duplicate
 * deliveries and bumps its generation only on an effective change.
 *
 * revision is passed as a double because JS numbers exceed 32-bit integer
 * range for a long-running session; it is narrowed to uint64 inside.
 */
EMSCRIPTEN_KEEPALIVE
void calypso_notify_viewport_observation_v1(
	double revision, int validMask,
	int physicalWidth, int physicalHeight,
	int logicalWidth, int logicalHeight,
	int safeTop, int safeRight, int safeBottom, int safeLeft,
	double dpr, int orientationCode)
{
	using namespace OpenXcom::Calypso;
	CalypsoViewportObservation obs;
	obs.revision = (revision > 0.0)
		? static_cast<std::uint64_t>(revision)
		: 0ull;
	obs.valid.physicalSize = (validMask & CALYPSO_VP_VALID_PHYSICAL)    != 0;
	obs.valid.logicalSize  = (validMask & CALYPSO_VP_VALID_LOGICAL)     != 0;
	obs.valid.safeArea     = (validMask & CALYPSO_VP_VALID_SAFE_AREA)   != 0;
	obs.valid.dpr          = (validMask & CALYPSO_VP_VALID_DPR)         != 0;
	obs.valid.orientation  = (validMask & CALYPSO_VP_VALID_ORIENTATION) != 0;
	obs.physicalWidth  = physicalWidth;
	obs.physicalHeight = physicalHeight;
	obs.logicalWidth   = logicalWidth;
	obs.logicalHeight  = logicalHeight;
	obs.safeTop    = safeTop;
	obs.safeRight  = safeRight;
	obs.safeBottom = safeBottom;
	obs.safeLeft   = safeLeft;
	obs.dpr = dpr;
	obs.orientation = orientationFromCode(orientationCode);
	calypsoHdViewportModel().applyObservation(obs);
}

} // extern "C"

#endif // __EMSCRIPTEN__
