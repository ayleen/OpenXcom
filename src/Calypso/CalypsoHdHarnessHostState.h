#pragma once
/*
 * Phase 46.4-F33 (Calypso) -- opaque-black engine harness host (F33-PARITY-002).
 *
 * The old comparison harness pushed a _screen = false AbandonGameState over
 * the visible MainMenuState and put the black backdrop INSIDE the target's
 * atomic subgroup, so any text failure rejected the backdrop and exposed the
 * menu. The host is STRUCTURAL instead: a full-canvas opaque-black state
 * (_screen = true) pushed BELOW the target preview, whose blackness never
 * depends on the target adapter committing anything.
 *
 * Lifecycle (driven by the pure CalypsoHdHarnessHostModel.h):
 *   1. the generic export validates the scenario id and requests the session;
 *   2. it pushes this host, then the target preview, exactly once;
 *   3. the target owns ordinary input and pop behavior;
 *   4. when the target closes, calypsoHdHarnessClose() clears the session and
 *      the host pops itself on its next think;
 *   5. repeated open requests never stack hosts or targets.
 *
 * Whole-file Emscripten guard (Phase 36 placement policy).
 */
#ifdef __EMSCRIPTEN__

#include "../Engine/State.h"
#include "CalypsoHdHarnessHostModel.h"

namespace OpenXcom
{
class Game;
class State;

namespace Calypso
{

/// Full-canvas opaque-black harness host. Sits below the target preview and
/// above every previous game state.
class CalypsoHdHarnessHostState : public State
{
public:
	explicit CalypsoHdHarnessHostState(CalypsoHarnessScenario scenario);

	void init() override;
	void think() override;
	void blit() override;

private:
	CalypsoHarnessScenario _scenario;
};

// --- Generic harness registry / session (family-agnostic) -------------------

/// The shared session of the active harness run (single, process-wide).
CalypsoHarnessSession& calypsoHarnessSession();

/// Create the target preview State for a stable scenario id. Returns nullptr
/// for unknown ids (the generic export never guesses).
State* calypsoHarnessCreateTarget(CalypsoHarnessScenario id);

/// Open the harness: session request -> push host -> push target once.
/// Returns false when the harness is already open or no game is live.
/// sideBySide selects the engine-side comparison shift (dialog left half).
bool calypsoHdHarnessOpen(CalypsoHarnessScenario id, CalypsoLayoutClass layout,
	bool sideBySide = true);

/// Close the harness (called by the target's teardown): clears the session;
/// the host pops itself on its next think.
void calypsoHdHarnessClose();

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
