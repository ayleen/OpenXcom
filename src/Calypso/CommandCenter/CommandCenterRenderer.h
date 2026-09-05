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
