#pragma once
/*
 * Command Center -- renderer (normative spec 2026-08-28, s.11/s.16-48).
 *
 * Emits the Command Center screen as HD overlay items through the shared
 * painter: root background, header (base selector and live date/time),
 * navigation rail with active indicator, clipped stage, zoom cluster,
 * and six-step time selector.
 * The selected-object/intercept panel is owned by the separate
 * interception flow and is intentionally absent here. Layout comes exclusively
 * from CommandCenterLayout; colours exclusively from CommandCenterTheme.
 *
 * Emscripten-only: consumed by CalypsoHdScreenRenderer's live/fixture
 * passes behind the command-center gate.
 */
#ifdef __EMSCRIPTEN__

#include "CommandCenterIcons.h"
#include "CommandCenterInteraction.h"
#include "CommandCenterLayout.h"
#include "CommandCenterTheme.h"

#include "../CalypsoF21UiShared.h"

#include <string>
#include <cstddef>
#include <vector>

namespace OpenXcom
{
class GeoscapeState;
}

namespace OpenXcom
{
namespace Calypso
{
namespace CommandCenter
{

/// Immutable per-frame data (spec s.2/s.61). The renderer never reads
/// backend state mid-frame; the caller builds this once.
struct CommandCenterSnapshot
{
	std::string baseCaption = "BASES";
	std::string baseName = "AURORA DAWN";
	std::vector<std::string> baseNames;
	std::size_t selectedBaseIndex = 0;
	bool baseSelectorOpen = false;
	std::string displayTime;
	std::string displayDate;
	int selectedTimeStep = 1; // index into the six canonical steps
};

/// Resolved font descriptors for the Command Center faces.
struct CommandCenterFonts
{
	CalypsoTtfSourceDescriptor interR;
	CalypsoTtfSourceDescriptor interM;
	CalypsoTtfSourceDescriptor interSb;
	CalypsoTtfSourceDescriptor plexR;
	CalypsoTtfSourceDescriptor plexM;
	CalypsoTtfSourceDescriptor plexSb;
	CalypsoTtfSourceDescriptor icons;
	bool ready = false;
};

/// Resolve the FONT_CC_* faces against the active mod. `icons` is optional
/// (the screen fails closed to label-only without the icon face).
CommandCenterFonts calypsoCcResolveFonts(const class Mod* mod);

/// Shared global-rail chrome (T07): header/rail backgrounds plus the five
/// section items with an explicit active section. Pure paint: no handler
/// calls, no state lookups, no widget bindings -- the caller binds its own
/// native widgets at these rects and passes labels it already owns (World
/// passes the kRailItems labels; Base will resolve live labels). Settings
/// stays pinned to the rail bottom in every composition.
void calypsoCcPaintHeaderBackground(CalypsoF21Painter& painter, const RectF& header,
	std::uint32_t& role);
void calypsoCcPaintRailBackground(CalypsoF21Painter& painter, const RectF& rail,
	std::uint32_t& role);
void calypsoCcPaintRailItems(CalypsoF21Painter& painter, const RectF& rail,
	RailAction active, const char* const labels[5],
	const CommandCenterFonts& fonts, std::uint32_t& role);
/// Shared rail section label (T14): index 0..4 WORLD/BASES/OPERATIONS/
/// ANALYTICS/ARCHIVE. Single definition site; empty string outside range.
const char* calypsoCcRailLabel(int index);

/// Shared header content (base chip + live date/time, spec s.19-21).
/// Geoscape and Basescape paint the same chrome from the same helper so the
/// header cannot drift: caption/name at layout.baseSelector, time/date at
/// layout.dateTimeBlock. interactive=false paints a display-only chip (no
/// chevron, no dropdown) for headers whose selector lives in the content,
/// like the Basescape MiniBaseView band. Pure paint: no handler calls.
void calypsoCcPaintHeaderContent(CalypsoF21Painter& painter,
	const CommandCenterLayout& layout, const CommandCenterSnapshot& content,
	const CommandCenterFonts& fonts, std::uint32_t& role, bool interactive = true);

/// Normalize one engine display string for TTF drawing: drop the OXCE inline
/// control tokens (TOK_COLOR_FLIP from {ALT}, TOK_NL_SMALL, TOK_CUSTOM_FORMAT
/// and the C0/C1 controls) that the raster bitmap font consumes as commands
/// but Inter/Plex render as tofu. Printable text, spaces, NBSP thousand
/// separators, and newlines pass through untouched; nothing is
/// string-specific, so funds, names, and regions share it. Pure and
/// byte-oriented (UTF-8 continuation bytes are never split).
inline std::string calypsoHdNormalizeTtfDisplayText(const std::string& text)
{
	std::string out;
	out.reserve(text.size());
	for (std::size_t i = 0; i < text.size();)
	{
		const unsigned char c = static_cast<unsigned char>(text[i]);
		if (c < 0x20)
		{
			// Keep LF as the only meaningful control; every other C0 byte
			// (including 0x01 TOK_COLOR_FLIP behind {ALT}) is an engine
			// command with no TTF glyph.
			if (c == 0x0A)
			{
				out.push_back(text[i]);
			}
			++i;
			continue;
		}
		if (c == 0x7F)
		{
			++i;
			continue;
		}
		if (c == 0xC2 && i + 1 < text.size())
		{
			const unsigned char next = static_cast<unsigned char>(text[i + 1]);
			if (next >= 0x80 && next <= 0x9F)
			{
				// C1 controls (U+0080..U+009F) have no Inter/Plex glyphs.
				i += 2;
				continue;
			}
		}
		out.push_back(text[i]);
		++i;
	}
	return out;
}

/// Emit the whole screen. `live` gates the world-region background (the
/// real globe pass owns it) and widget claim binding; `state` may be null
/// in fixture mode. Every draw consumes `role` in sequence.
void calypsoCcRender(CalypsoF21Painter& painter, const CommandCenterLayout& layout,
	const CommandCenterSnapshot& snapshot, const CommandCenterFonts& fonts,
	const CalypsoHdPresentationMetrics& metrics, bool live, GeoscapeState* state, std::uint32_t& role);

/// Ordinary gameplay enables Command Center; harnesses keep explicit routes.
bool calypsoCcEnabled();
void calypsoCcSetEnabled(bool on);

/// Physical-pixel stage rect published by the screen renderer each CC frame
/// and consumed by the globe direct pass (stage 7 clipping).
struct CcStageRect
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	bool active = false;
};
void calypsoCcSetStageRect(const CcStageRect& rect);
CcStageRect calypsoCcStageRect();

} // namespace CommandCenter
} // namespace Calypso
} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
