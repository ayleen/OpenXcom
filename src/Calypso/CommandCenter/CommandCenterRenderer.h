#pragma once
/*
 * Command Center -- renderer (normative spec 2026-08-28, s.11/s.16-48).
 *
 * Emits the Command Center screen as HD overlay items through the shared
 * painter: root background, header (session selector, date/time, divider,
 * system status, bell), navigation rail with active indicator, clipped
 * stage frame, zoom cluster, timeline (play/pause, time steps, ruler,
 * fullscreen) and the inspector panel. Layout comes exclusively from
 * CommandCenterLayout; colours exclusively from CommandCenterTheme.
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
	std::string sessionCaption = "SESSION";
	std::string sessionName = "AURORA DAWN";
	std::string displayTime;
	std::string displayDate;
	std::string systemStatus = "NOMINAL";
	bool systemNominal = true;
	bool simulationPlaying = true;
	int selectedTimeStep = 1; // index into the six canonical steps
	bool hasUnreadNotification = true;
	bool inspectorOpen = true;
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
	bool live, GeoscapeState* state, std::uint32_t& role);

/// Runtime gate (loopback QA param ?cc=1 -> main.js ccall).
bool calypsoCcEnabled();
void calypsoCcSetEnabled(bool on);

} // namespace CommandCenter
} // namespace Calypso
} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
