#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- Emscripten bridge + poll adapter around the
 * portable CalypsoViewportModel.
 *
 * Owns the single process-wide HD viewport model instance and the two
 * producers that feed it:
 *   1. the JS bridge C ABI `calypso_notify_viewport_observation_v1(...)`
 *      (published from web/src/main.js alongside the legacy
 *      `calypso_notify_viewport`), authoritative for every VALID field;
 *   2. `calypsoHdViewportBackingStorePoll()`, a once-per-frame poll of the
 *      canvas backing-store size (physical dimensions only), covering canvas
 *      changes that arrive with no browser event.
 *
 * It also builds the ONE frozen per-frame presentation-metrics snapshot from
 * the current model state and the engine's logical base resolution. This whole
 * file is Emscripten-only; the portable model it wraps is natively tested.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoViewportModel.h"
#include "CalypsoHdUiModel.h"

namespace OpenXcom
{
namespace Calypso
{

/// Bit flags for the JS observation validity mask (must match the values used
/// in web/src/main.js calypso_notify_viewport_observation_v1 publication).
enum CalypsoViewportValidBit
{
	CALYPSO_VP_VALID_PHYSICAL    = 1 << 0,
	CALYPSO_VP_VALID_LOGICAL     = 1 << 1,
	CALYPSO_VP_VALID_SAFE_AREA   = 1 << 2,
	CALYPSO_VP_VALID_DPR         = 1 << 3,
	CALYPSO_VP_VALID_ORIENTATION = 1 << 4,
};

/// Orientation codes carried across the ABI (0 unknown, 1 portrait,
/// 2 landscape). Mapped to CalypsoOrientation inside the bridge.
enum CalypsoViewportOrientationCode
{
	CALYPSO_VP_ORIENT_UNKNOWN   = 0,
	CALYPSO_VP_ORIENT_PORTRAIT  = 1,
	CALYPSO_VP_ORIENT_LANDSCAPE = 2,
};

/// The single HD viewport model instance (bridge-owned).
CalypsoViewportModel& calypsoHdViewportModel();

/// Apply a once-per-frame backing-store poll (physical canvas dims only).
/// Returns true iff the physical dimensions changed.
bool calypsoHdViewportBackingStorePoll(int physicalWidth, int physicalHeight);

/// Build the frozen presentation-metrics snapshot for this frame from the
/// current model state and the engine's logical base resolution. Uses the
/// model generation as the metrics generation so a stale frame is detectable.
CalypsoHdPresentationMetrics calypsoHdBuildPresentationMetrics(
	int logicalWidth, int logicalHeight);

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
