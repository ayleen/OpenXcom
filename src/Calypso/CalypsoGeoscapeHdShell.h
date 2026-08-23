#ifdef __EMSCRIPTEN__
#ifndef OPENXCOM_CALYPSOGEOSCAPEHDSHELL_H
#define OPENXCOM_CALYPSOGEOSCAPEHDSHELL_H

/*
 * Phase 46.4 Stage 9 (Calypso) — responsive strategic command shell.
 * Whole file is Emscripten-only, mirroring CalypsoGeoscapeHd.
 *
 * The shell does NOT build parallel gameplay widgets: it re-projects the
 * existing GeoscapeState affordances (nav buttons, time controls, zoom)
 * from the generated strategic-command-shell contract through the Stage 8b
 * projection, and hides only the legacy side-panel fillers it replaces.
 * Handlers, timers, globe, popups stay authoritative in GeoscapeState.
 */

#include <string>

namespace OpenXcom
{


class GeoscapeState;
class Surface;

struct CalypsoGeoscapeHdShell
{
	/// Re-project shell widgets from the contract when the F16 gate is on.
	/// Returns the gate decision reason for diagnostics. Safe to call again
	/// on resize; idempotent while the viewport generation is unchanged.
	static const char* apply(GeoscapeState *s);
	/// Friend-granted widget resolution; must be a member so the
	/// GeoscapeState friendship applies.
	static Surface *resolveWidget(GeoscapeState *s, const std::string& member);
};

}

#endif
#endif