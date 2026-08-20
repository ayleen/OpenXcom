/*
 * Phase 46.4-F33 (Calypso) -- portable tracked single-line TTF composition.
 * See CalypsoHdTrackedText.h.
 */
#include "CalypsoHdTrackedText.h"

#include <algorithm>
#include <cstddef>

namespace OpenXcom
{
namespace Calypso
{
namespace
{

/// Decode one UTF-8 codepoint at `i` in `s` (size n). Returns false on
/// malformed input.
bool decodeUtf8At(const unsigned char* s, std::size_t n, std::size_t i,
	std::uint32_t& cp, std::size_t& len)
{
	const unsigned char b0 = s[i];
	if (b0 < 0x80u) { cp = b0; len = 1; }
	else if ((b0 & 0xE0u) == 0xC0u && i + 1 < n)
	{
		cp = ((b0 & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu); len = 2;
	}
	else if ((b0 & 0xF0u) == 0xE0u && i + 2 < n)
	{
		cp = ((b0 & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) | (s[i + 2] & 0x3Fu); len = 3;
	}
	else if ((b0 & 0xF8u) == 0xF0u && i + 3 < n)
	{
		cp = ((b0 & 0x07u) << 18) | ((s[i + 1] & 0x3Fu) << 12)
			| ((s[i + 2] & 0x3Fu) << 6) | (s[i + 3] & 0x3Fu); len = 4;
	}
	else
	{
		return false;
	}
	return true;
}

void encodeUtf8(std::uint32_t cp, char out[5])
{
	if (cp < 0x80u)
	{
		out[0] = (char)cp; out[1] = 0;
	}
	else if (cp < 0x800u)
	{
		out[0] = (char)(0xC0u | (cp >> 6)); out[1] = (char)(0x80u | (cp & 0x3Fu)); out[2] = 0;
	}
	else if (cp < 0x10000u)
	{
		out[0] = (char)(0xE0u | (cp >> 12)); out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
		out[2] = (char)(0x80u | (cp & 0x3Fu)); out[3] = 0;
	}
	else
	{
		out[0] = (char)(0xF0u | (cp >> 18)); out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
		out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu)); out[3] = (char)(0x80u | (cp & 0x3Fu)); out[4] = 0;
	}
}

} // namespace

bool calypsoFaceCoversText(TTF_Font* face, const std::string& text)
{
	const unsigned char* s = reinterpret_cast<const unsigned char*>(text.c_str());
	const std::size_t n = text.size();
	std::size_t i = 0;
	while (i < n)
	{
		std::uint32_t cp;
		std::size_t len;
		if (!decodeUtf8At(s, n, i, cp, len)) return false; // malformed -> uncovered
		i += len;
		if (cp <= 0x20u) continue;      // space / control / newline: not a glyph
		if (cp > 0xFFFFu) return false; // astral: cannot BMP-probe -> safe fallback
		if (!TTF_GlyphIsProvided(face, static_cast<Uint16>(cp))) return false;
	}
	return true;
}

SDL_Surface* calypsoRasterTracked(TTF_Font* face, const std::string& text,
	int trackingPx, SDL_Color color)
{
	if (!face || text.empty() || trackingPx < 0) return nullptr;

	// Missing glyph coverage or malformed UTF-8 rejects the WHOLE string: the
	// generator would otherwise emit .notdef tofu for the covered parts and a
	// broken slice for the rest (F33-PARITY-001/#2: fail closed, never tofu).
	if (!calypsoFaceCoversText(face, text)) return nullptr;

	// Measure the space advance (SDL_ttf convention: 0 == success).
	int minx = 0, maxx = 0, miny = 0, maxy = 0;
	int spaceAdvance = 0;
	if (TTF_GlyphMetrics(face, static_cast<Uint16>(u' '), &minx, &maxx, &miny, &maxy, &spaceAdvance) != 0)
	{
		spaceAdvance = TTF_FontHeight(face) / 3; // defensive fallback; metrics rarely fail
	}

	const unsigned char* s = reinterpret_cast<const unsigned char*>(text.c_str());
	const std::size_t n = text.size();

	// Pass 1: measure per line (advance + tracking, no trailing tracking).
	int maxWidth = 1;
	int lineCount = 1;
	{
		int pen = 0;
		std::size_t i = 0;
		while (i < n)
		{
			std::uint32_t cp; std::size_t len;
			if (!decodeUtf8At(s, n, i, cp, len)) return nullptr;
			i += len;
			if (cp == u'\n') { maxWidth = std::max(maxWidth, pen); pen = 0; ++lineCount; continue; }
			if (cp <= 0x20u) { pen += spaceAdvance + trackingPx; continue; }
			int advance = 0;
			if (TTF_GlyphMetrics(face, static_cast<Uint16>(cp), &minx, &maxx, &miny, &maxy, &advance) != 0)
			{
				return nullptr; // real SDL_ttf metric failure -> fail closed
			}
			pen += advance + trackingPx;
		}
		maxWidth = std::max(maxWidth, pen - trackingPx);
	}

	const int lineSkip = TTF_FontLineSkip(face);
	const int fontHeight = TTF_FontHeight(face);
	const int height = lineCount <= 1 ? fontHeight : (lineCount - 1) * lineSkip + fontHeight;

	SDL_Surface* canvas = SDL_CreateRGBSurfaceWithFormat(0, maxWidth, height, 32,
		SDL_PIXELFORMAT_ARGB8888);
	if (!canvas) return nullptr;

	// Pass 2: compose. Every per-glyph render/blit result is checked; any real
	// failure disposes the canvas and fails the whole string (no partial claim).
	{
		int x = 0, y = 0;
		std::size_t i = 0;
		while (i < n)
		{
			std::uint32_t cp; std::size_t len;
			if (!decodeUtf8At(s, n, i, cp, len)) { SDL_FreeSurface(canvas); return nullptr; }
			i += len;
			if (cp == u'\n') { x = 0; y += lineSkip; continue; }
			if (cp <= 0x20u) { x += spaceAdvance + trackingPx; continue; }

			char utf8[5];
			encodeUtf8(cp, utf8);
			SDL_Surface* glyph = TTF_RenderUTF8_Blended(face, utf8, color);
			if (!glyph) { SDL_FreeSurface(canvas); return nullptr; }
			// Copy, don't alpha-blend: the canvas is transparent and glyph boxes
			// advance past each other, so overwrite is exact.
			SDL_SetSurfaceBlendMode(glyph, SDL_BLENDMODE_NONE);
			SDL_Rect dst{ x, y, glyph->w, glyph->h };
			if (SDL_BlitSurface(glyph, nullptr, canvas, &dst) != 0)
			{
				SDL_FreeSurface(glyph);
				SDL_FreeSurface(canvas);
				return nullptr;
			}
			SDL_FreeSurface(glyph);

			int advance = 0;
			if (TTF_GlyphMetrics(face, static_cast<Uint16>(cp), &minx, &maxx, &miny, &maxy, &advance) != 0)
			{
				SDL_FreeSurface(canvas);
				return nullptr;
			}
			x += advance + trackingPx;
		}
	}
	return canvas;
}

} // namespace Calypso
} // namespace OpenXcom
