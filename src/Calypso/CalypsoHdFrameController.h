#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- portable frame-lifecycle controller for the HD
 * UI overlay.
 *
 * Owns the per-frame decision "may this frame present HD physical visuals?"
 * plus the whole failure policy that replaces the archive's fence/epoch
 * machinery: one monotonic frame id, one boolean `forceLogicalNextFrame`
 * latch, and the frame-scoped claim set. There is no epoch, no acknowledgment
 * channel, no rollback graph -- a failure latches exactly one wholly-logical
 * frame and clears.
 *
 * Failure policy (mirrors the plan "Decision" section):
 *   - Before claims (prep/upload not Ready): the caller simply does not
 *     commit that subgroup's claims; nothing here changes.
 *   - After claims (a post-claim GL/SDL draw failure): notePostClaimFailure()
 *     discards this frame's claims, latches forceLogicalNextFrame, and the
 *     caller skips SDL_RenderPresent for this frame.
 *   - Context loss: noteContextLost() suspends physical output; the first
 *     presentable frame after noteContextRestored() is wholly logical, then
 *     HD may warm again.
 *
 * Dependency-free (only <cstdint> via CalypsoHdUiModel.h): natively unit
 * tested, no SDL/GL/browser/engine deps, not #ifdef-guarded. The Emscripten
 * queue (CalypsoHdUiOverlay) drives one instance; native OXCE is unaffected.
 */
#include "CalypsoHdUiModel.h"

namespace OpenXcom
{
namespace Calypso
{

class CalypsoHdFrameController
{
public:
	struct BeginResult
	{
		std::uint64_t frameId = 0;
		bool mayGoPhysical = false;
	};

	/// Open a new presentable frame. Advances the frame id, clears the claim
	/// set for the new frame, and decides whether HD physical output is
	/// permitted. Physical is denied (fully logical frame) when context is
	/// lost, the presentation metrics are invalid, or a prior failure/restore
	/// latched forceLogicalNextFrame -- and that latch is CONSUMED here so the
	/// following frame may warm again.
	BeginResult beginFrame(bool metricsValid)
	{
		++_frameId;
		_claims.beginFrame(_frameId);
		const bool physical = !_contextLost && metricsValid && !_forceLogicalNextFrame;
		_forceLogicalNextFrame = false;
		BeginResult r;
		r.frameId = _frameId;
		r.mayGoPhysical = physical;
		return r;
	}

	/// A draw failure inside a claimed stage AFTER claims committed. Discards
	/// this frame's claims (so the widgets that were suppressed render logical
	/// again -- though this frame is already lost, the caller skips present),
	/// and latches one wholly-logical next frame.
	void notePostClaimFailure()
	{
		_claims.clear();
		_forceLogicalNextFrame = true;
	}

	/// WebGL context lost: suspend physical output and drop frame claims.
	void noteContextLost()
	{
		_contextLost = true;
		_claims.clear();
		_forceLogicalNextFrame = true;
	}

	/// WebGL context restored: allow physical output again, but latch one
	/// wholly-logical frame so caches re-warm before anything is claimed.
	void noteContextRestored()
	{
		_contextLost = false;
		_forceLogicalNextFrame = true;
	}

	CalypsoHdClaimSet& claims() { return _claims; }
	const CalypsoHdClaimSet& claims() const { return _claims; }

	std::uint64_t frameId() const { return _frameId; }
	bool contextLost() const { return _contextLost; }
	bool forceLogicalNextFrame() const { return _forceLogicalNextFrame; }

private:
	CalypsoHdClaimSet _claims;
	std::uint64_t _frameId = 0;
	bool _contextLost = false;
	bool _forceLogicalNextFrame = false;
};

} // namespace Calypso
} // namespace OpenXcom
