#pragma once
/*
 * Phase 46.4-F33 (Calypso) -- portable tracked single-line TTF composition.
 *
 * SDL_ttf has no letter-spacing, so tracked labels ("ABANDON GAME?",
 * "OPTIONS", button captions) are composed per glyph: each codepoint is
 * rendered as a one-character string (bearing math stays SDL_ttf's own) and
 * copied onto a transparent ARGB canvas, advancing by glyph advance + tracking.
 *
 * This is the ONLY implementation of tracked composition, shared by the
 * browser rasteriser (CalypsoHdTextRaster.cpp, #ifdef __EMSCRIPTEN__) and the
 * native doctest suite -- which is what lets a real SDL_ttf-backed native test
 * pin the F33-PARITY-001 defect: SDL_ttf returns 0 on SUCCESS, and the old
 * code tested !result, so every valid tracked label failed every frame.
 *
 * Deliberately NOT wrapped in #ifdef __EMSCRIPTEN__: it compiles and runs
 * natively (SDL_ttf is a native dependency via TTFFont) and is exercised by
 * the native unit tests. Straight (non-premultiplied) alpha throughout --
 * the same convention the HD UI overlay blends with.
 */
#include <cstdint>
#include <string>

#include <SDL.h>
#include <SDL_ttf.h>

namespace OpenXcom
{
namespace Calypso
{

/// True iff every renderable codepoint in `text` has a real glyph in `face`.
/// Whitespace / control codepoints (<= 0x20) are layout, not glyphs, and are
/// skipped; astral codepoints (> 0xFFFF) cannot be probed with the BMP-only
/// TTF_GlyphIsProvided and are conservatively UNCOVERED (safe direction:
/// logical fallback, never tofu). Malformed UTF-8 also reports false.
bool calypsoFaceCoversText(TTF_Font* face, const std::string& text);

/// Compose `text` on a fresh straight-alpha ARGB8888 surface, applying
/// `trackingPx` between glyphs (no trailing tracking). Line breaks (`\n`)
/// are honored; wrapped text uses the caller's wrapped path, never here.

/// Returns nullptr ONLY on real SDL_ttf errors, missing glyph coverage, or
/// malformed UTF-8 -- a valid tracked label must NEVER fail (F33-PARITY-001).
/// The result is owned by the caller (SDL_FreeSurface).
SDL_Surface* calypsoRasterTracked(TTF_Font* face, const std::string& text,
	int trackingPx, SDL_Color color);

} // namespace Calypso
} // namespace OpenXcom
