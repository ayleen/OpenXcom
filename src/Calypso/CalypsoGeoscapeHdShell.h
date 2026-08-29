#ifdef __EMSCRIPTEN__
#ifndef OPENXCOM_CALYPSOGEOSCAPEHDSHELL_H
#define OPENXCOM_CALYPSOGEOSCAPEHDSHELL_H

/* Phase 46.4 Stage 9 (Calypso) — responsive strategic command shell.
 * Re-projects existing GeoscapeState affordances from the generated
 * strategic-command-shell contract and owns the secondary-route drawer
 * container (Stage 9.1.3). Handlers stay authoritative in GeoscapeState. */

#include <string>
#include <cstddef>

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
	static bool isLiveActionVisible(const GeoscapeState *s, const std::string& actionId);
	static Surface* resolveBaseSelectorRow(GeoscapeState *s, std::size_t index);
	static bool isBaseSelectorOpen(const GeoscapeState *s);
	static std::size_t selectedBaseIndex(const GeoscapeState *s);
	static CalypsoGeoscapeHdShellState* state(GeoscapeState *s);
	static void applyPendingBaseFocus(GeoscapeState *s);

	/// Toggle/close the state-owned secondary drawer or Command Center base
	/// selector. Both remain scoped to this GeoscapeState instance.
	static void toggleDrawer(GeoscapeState *s);
	static bool closeDrawer(GeoscapeState *s);

	/// Reason-aware pause ownership (audit §13 item 1). `togglePause` flips
	/// one User ledger token; `syncPause` re-derives the authoritative vanilla
	/// `_pause` latch from the frame's system reason plus the persistent
	/// ledger; `effectivePause` reads the same truth without mutating.
	static void togglePause(GeoscapeState *s);
	static void syncPause(GeoscapeState *s, bool systemReason);
	static bool effectivePause(const GeoscapeState *s);
	static bool isDrawerOpen(const GeoscapeState *s);

	/// Release per-state shell bookkeeping before State destroys its surfaces.
	static void destroy(GeoscapeState *s);

}; /* struct */

} /* namespace OpenXcom */

#endif
#endif /* __EMSCRIPTEN__ */
