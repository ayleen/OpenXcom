#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- frame-scoped logical-visual suppression for
 * opted-in widgets.
 *
 * When a family adapter takes over a widget's visual for a frame (submitting a
 * physical HD replacement via CalypsoHdUiOverlay::submitText / a painter and
 * claiming it), it registers the live widget pointer here for that frame. The
 * widget's own draw path asks whether it is claimed and, if so, renders
 * nothing into the logical surface -- so the overlay's physical version is the
 * only thing on screen, with no double-draw and no logical bleed.
 *
 * The claim is strictly frame-scoped: a query only matches when the frame id
 * equals the frame the claim was made in, so a stale claim can never suppress
 * a widget on a later frame (complete logical fallback is automatic). The
 * registry self-clears when the frame id advances. Whole-file Emscripten
 * guard; no native effect.
 */
#ifdef __EMSCRIPTEN__

#include <cstdint>

namespace OpenXcom
{
namespace Calypso
{

/// Claim `widget`'s logical visual for `frameId`. Advancing the frame id clears
/// all prior claims. A null widget is ignored.
void calypsoHdWidgetClaim(const void* widget, std::uint64_t frameId);

/// True iff `widget` was claimed for exactly this `frameId`. A frame mismatch
/// (or a null widget) returns false -- the fail-safe to logical rendering.
bool calypsoHdWidgetClaimed(const void* widget, std::uint64_t frameId);

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
