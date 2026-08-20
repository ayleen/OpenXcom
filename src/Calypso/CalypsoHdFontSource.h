#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- resolves a mod-registered TTFFont id into the
 * portable CalypsoTtfSourceDescriptor identity used by CalypsoHdTextRasterKey.
 * Emscripten-only: the whole-file guard matches the Phase 36 placement policy
 * (see CalypsoViewportMailbox.cpp for the reference shape).
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdTextRasterKey.h"

#include <cstdint>
#include <string>

namespace OpenXcom
{
class Mod;
}

namespace OpenXcom
{
namespace Calypso
{

/// Current global font-resource generation. Bumped whenever a mod reload (or
/// other VFS invalidation) may have replaced the bytes behind a previously
/// resolved CalypsoTtfSourceDescriptor, so stale rasters/textures keyed on the
/// old generation stop matching.
std::uint64_t calypsoHdFontResourceGeneration();

/// Increments the global font-resource generation. No caller yet (Phase
/// 46.2-HD.3 lands the plumbing only); a future mod-reload hook will call
/// this so in-flight HD text caches invalidate correctly.
void calypsoHdBumpFontResourceGeneration();

/// Resolves `fontId` (an extraTTFFonts id) against `mod`'s registered
/// TTFFont, filling `out` with its canonical VFS path, the registered Unicode
/// fallback path when available, face index (always 0 -- TTFFont wraps a
/// single-face TTF_Font), logical design size, and the current font-resource
/// generation. Returns false (leaving `out`
/// unspecified) if `mod` is null or the font id is not registered.
bool calypsoHdResolveFontDescriptor(const Mod* mod, const std::string& fontId,
	CalypsoTtfSourceDescriptor& out);

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
