#ifdef __EMSCRIPTEN__
#ifndef OPENXCOM_CALYPSOGEOSCAPEHDSHELL_H
#define OPENXCOM_CALYPSOGEOSCAPEHDSHELL_H

/* Phase 46.4 Stage 9 (Calypso) — responsive strategic command shell.
 * Re-projects existing GeoscapeState affordances from the generated
 * strategic-command-shell contract and owns the secondary-route drawer
 * container (Stage 9.1.3). Handlers stay authoritative in GeoscapeState. */

#include <string>

namespace OpenXcom
{

class GeoscapeState;
class Surface;

struct CalypsoGeoscapeHdShell
{
	/// Re-project shell widgets from the contract when the F16 gate is on.
	static const char* apply(GeoscapeState *s);
	static Surface* resolveWidget(GeoscapeState *s, const std::string& member);

	/// Toggle the secondary-route drawer (action.session affordance).
	static void toggleDrawer(GeoscapeState *s);

}; /* struct */

} /* namespace OpenXcom */

#endif
#endif /* __EMSCRIPTEN__ */