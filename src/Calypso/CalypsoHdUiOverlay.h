#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- the screen-owned HD UI overlay queue.
 *
 * One instance for the renderer's lifetime. It owns the per-frame lifecycle
 * (CalypsoHdFrameController), the ONE frozen presentation-metrics snapshot for
 * the current frame, and -- once a family adapter submits an enabled group --
 * the shared GPU resources, bounded raster/texture caches, frame claims, and
 * the ordered HD UI + diagnostics stages.
 *
 * This checkpoint (HD.2) lands the lifecycle skeleton and the two Screen
 * seams: beginFrame() freezes metrics + advances the frame at the top of
 * Screen::flip(), and renderStages() runs after the legacy composite. With no
 * enabled group the queue is DORMANT -- every entry early-returns and native
 * behaviour is byte-for-byte unchanged. Widget adapters, physical text upload,
 * and the drawing stages arrive in HD.3/HD.4.
 *
 * Whole-file Emscripten guard (Phase 36 placement policy). The heavy logic it
 * composes -- CalypsoHdFrameController, CalypsoHdPresentationMetrics,
 * CalypsoViewportModel -- is portable and natively tested.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdFrameController.h"
#include "CalypsoHdUiModel.h"

namespace OpenXcom
{
namespace Calypso
{

class CalypsoHdUiOverlay
{
public:
	static CalypsoHdUiOverlay& instance();

	/// Top of Screen::flip(): poll the canvas backing store, freeze ONE
	/// presentation-metrics snapshot for this frame, and advance the frame
	/// controller. `logicalWidth/Height` are the engine's logical base
	/// resolution (Options::baseX/YResolution). After this returns nothing may
	/// remix the frozen metrics until the next frame.
	void beginFrame(int logicalWidth, int logicalHeight);

	/// After the legacy composite in Screen::flip(): run the ordered HD UI and
	/// diagnostics stages. Dormant (no-op) until an adapter submits an enabled
	/// group. Returns false if a post-claim draw failure occurred (the caller
	/// then skips SDL_RenderPresent); true otherwise.
	bool renderStages();

	/// WebGL context lost/restored -- forwarded to the frame controller. When
	/// GL resources exist (HD.3+) this is driven by the ShaderManager
	/// reset-callback ladder; until then it is a safe no-op on the lifecycle.
	void contextLost();
	void contextRestored();

	const CalypsoHdPresentationMetrics& frozenMetrics() const { return _frozenMetrics; }
	bool mayGoPhysical() const { return _mayGoPhysical; }
	std::uint64_t frameId() const { return _controller.frameId(); }

	/// True once at least one adapter has an enabled group this frame. HD.2:
	/// always false (no adapters yet), which is what keeps the queue dormant.
	bool hasEnabledGroups() const { return _enabledGroupCount > 0; }

private:
	CalypsoHdUiOverlay() = default;

	CalypsoHdFrameController _controller;
	CalypsoHdPresentationMetrics _frozenMetrics;
	bool _mayGoPhysical = false;
	int _enabledGroupCount = 0; // adapters bump this in HD.4; 0 => dormant
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
