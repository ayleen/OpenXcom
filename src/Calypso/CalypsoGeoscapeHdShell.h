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
struct CalypsoGeoscapeHdShellState;

struct CalypsoGeoscapeHdShell
{
	/// Re-project shell widgets from the contract when the F16 gate is on.
	static const char* apply(GeoscapeState *s);
	static const Surface* resolveWidget(const GeoscapeState *s, const std::string& member);
	static Surface* resolveWidget(GeoscapeState *s, const std::string& member);
	static const Surface* resolveLiveWidget(const GeoscapeState *s, const std::string& actionId);
	static Surface* resolveLiveWidget(GeoscapeState *s, const std::string& actionId);
	static CalypsoGeoscapeHdShellState* state(GeoscapeState *s);

	/// Toggle the secondary-route drawer (action.session affordance).
	static void toggleDrawer(GeoscapeState *s);

	/// Release per-state shell bookkeeping before State destroys its surfaces.
	static void destroy(GeoscapeState *s);

}; /* struct */

} /* namespace OpenXcom */

#endif
#endif /* __EMSCRIPTEN__ */
