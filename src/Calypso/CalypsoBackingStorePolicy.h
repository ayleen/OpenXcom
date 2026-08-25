#pragma once
/*
 * Phase 46.4 10.2.9 -- pure backing-store ownership policy.
 *
 * The browser canvas backing store is owned, not assumed. Two pure decisions
 * live here so native doctests exercise the real policy without SDL,
 * JavaScript, or a browser:
 *
 *   1. calypsoExpectedBackingStore mirrors the web shell's CSS->physical
 *      mapping (web/public/backing-store.js + web/src/main.js
 *      sizeForViewport): DPR clamped into [1, 2], CSS pixels rounded
 *      half-up, hard minimum floors of 320x200.
 *   2. calypsoClassifyCanvasResize decides what the engine's per-frame
 *      canvas poll must do when the polled canvas differs from
 *      Options::displayWidth/Height. A canvas size that no explicitly
 *      pending viewport resize EXACTLY matches is a hostile browser rewrite
 *      that dropped the DPR factor; it must be RESTORED to the authoritative
 *      physical size and never adopted, aspect-clamped, or reduced to CSS
 *      pixels. Committed runtime state is NOT an authorization: the HD UI
 *      overlay's earlier per-frame poll can already have observed (and
 *      echoed) a hostile reduced canvas, so only the queued viewport
 *      transition captured before classification may authorize adoption.
 */
#include <cmath>

namespace OpenXcom
{
namespace Calypso
{

/// Hard floors mirrored from web/public/backing-store.js / main.js.
static const int CALYPSO_BACKING_MIN_WIDTH = 320;
static const int CALYPSO_BACKING_MIN_HEIGHT = 200;
/// Same DPR ceiling as the production web shell (docs/hd-ui-overlay-pipeline.md).
static const double CALYPSO_BACKING_MAX_DPR = 2.0;

struct CalypsoBackingStoreSize
{
	int width = 0;
	int height = 0;
	double dpr = 1.0;
};

/// The exact physical backing store a healthy browser session must hold for
/// the given CSS viewport. Mirrors Math.min(devicePixelRatio||1, 2) and
/// max(floor, round(css*dpr)) from the web shell bit for bit.
inline CalypsoBackingStoreSize calypsoExpectedBackingStore(
	double cssWidth, double cssHeight, double devicePixelRatio)
{
	const double rawDpr = (std::isfinite(devicePixelRatio) && devicePixelRatio > 0.0)
		? devicePixelRatio : 1.0;
	CalypsoBackingStoreSize size;
	size.dpr = rawDpr > CALYPSO_BACKING_MAX_DPR ? CALYPSO_BACKING_MAX_DPR : rawDpr;
	// Math.round rounds half toward positive infinity for these magnitudes.
	const double scaledWidth = std::floor(cssWidth * size.dpr + 0.5);
	const double scaledHeight = std::floor(cssHeight * size.dpr + 0.5);
	size.width = static_cast<int>(scaledWidth > CALYPSO_BACKING_MIN_WIDTH
		? scaledWidth : CALYPSO_BACKING_MIN_WIDTH);
	size.height = static_cast<int>(scaledHeight > CALYPSO_BACKING_MIN_HEIGHT
		? scaledHeight : CALYPSO_BACKING_MIN_HEIGHT);
	return size;
}

enum class CalypsoCanvasMismatchAction
{
	None,    ///< canvas already equals the authoritative physical size
	Adopt,   ///< a viewport notification authorized exactly this canvas size
	Restore, ///< unauthorized divergence: reassert the physical size
};

/// Which bridged source may authorize a polled canvas size: ONLY an
/// explicitly queued (not yet consumed) viewport transition. The caller
/// captures it from calypsoPendingViewportSize immediately before
/// classification; committed runtime physical size is deliberately absent
/// because the HD overlay's per-frame poll can already have observed a
/// hostile reduced canvas.
struct CalypsoCanvasAuthorization
{
	/// True only when a viewport transition is queued and unconsumed.
	bool hasPendingViewport = false;
	/// Exact physical size carried by that queued transition.
	int pendingWidth = 0;
	/// Exact physical height carried by that queued transition.
	int pendingHeight = 0;

	static CalypsoCanvasAuthorization none() { return CalypsoCanvasAuthorization{}; }
};

/// Classify one polled canvas observation against the engine's authoritative
/// physical display size. Adoption requires an EXACT match with an explicitly
/// pending viewport resize (checked here, inside the policy, so no caller can
/// widen it); every other divergence restores it. Aspect ratios are never
/// consulted -- there is no clamp that could soften a verdict.
inline CalypsoCanvasMismatchAction calypsoClassifyCanvasResize(
	int canvasWidth, int canvasHeight,
	int displayWidth, int displayHeight,
	const CalypsoCanvasAuthorization& authorization)
{
	if (canvasWidth <= 0 || canvasHeight <= 0) return CalypsoCanvasMismatchAction::None;
	if (displayWidth <= 0 || displayHeight <= 0) return CalypsoCanvasMismatchAction::None;
	if (canvasWidth == displayWidth && canvasHeight == displayHeight)
		return CalypsoCanvasMismatchAction::None;
	if (authorization.hasPendingViewport
	    && authorization.pendingWidth == canvasWidth
	    && authorization.pendingHeight == canvasHeight)
		return CalypsoCanvasMismatchAction::Adopt;
	return CalypsoCanvasMismatchAction::Restore;
}

} // namespace Calypso
} // namespace OpenXcom
