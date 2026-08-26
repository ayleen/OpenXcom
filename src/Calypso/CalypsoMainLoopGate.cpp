#ifdef __EMSCRIPTEN__
/*
 * Calypso main-loop / WebGL context-loss gate (Phase 46.1 / M6c) -- extracted
 * verbatim from Engine/EmscriptenHarness.cpp (policy R3 / R6 relocation-only).
 *
 * Owns the viewport-blocked + main-loop started/paused flags and the
 * g_calypsoContextLost C-linkage flag, together with their pause/resume logic
 * and the JS-facing EMSCRIPTEN_KEEPALIVE exports. extern "C" symbol names and
 * ABI are preserved exactly (Screen::flip declares `extern "C" int
 * g_calypsoContextLost;`); see `git diff --color-moved=dimmed-zebra`.
 */
#include <emscripten.h>
#include <SDL.h>

extern "C" {

static int s_calypsoViewportBlocked = 0;
static int s_calypsoMainLoopStarted = 0;
static int s_calypsoMainLoopPaused = 0;
static int s_calypsoRecoveryPending = 0;
static int s_calypsoResetSentinelPending = 0;
static int s_calypsoResetBoundaryOpen = 0;
static int s_calypsoTimingReady = 1;
extern int g_calypsoContextLost;
void calypso_restart_main_loop(void);

void calypso_reset_main_loop_state(void)
{
	s_calypsoMainLoopStarted = 0;
	s_calypsoMainLoopPaused = 0;
	s_calypsoRecoveryPending = 0;
	s_calypsoResetSentinelPending = 0;
	s_calypsoResetBoundaryOpen = 0;
	s_calypsoTimingReady = 1;
}

int calypso_pause_main_loop_before_iterate(void)
{
	// Reaching the callback is the first authoritative proof that Emscripten has
	// installed MainLoop.func. A flag set before set_main_loop_arg is too early:
	// JS can run between those points and resume a still-null loop.
	s_calypsoMainLoopStarted = 1;
	/* Restore installs one recovery tick while the normal gate remains paused.
	 * Screen::handle consumes SDL_RENDER_TARGETS_RESET on that tick; flip() is
	 * still blocked by g_calypsoContextLost until the transaction commits. */
	if (s_calypsoRecoveryPending)
	{
		// Keep the transaction pending until Screen::handle consumes the reset
		// event and commits renderer/context/resource recovery.  A distinct
		// return code lets Game::emscriptenIter run only the recovery tick;
		// returning the normal-frame code here would enter Game::iterate().
		return 2;
	}
	s_calypsoTimingReady = 1;
	if (!s_calypsoViewportBlocked && !g_calypsoContextLost)
		return 0;
	// pause_main_loop only prevents future callbacks; the current callback must
	// return explicitly so no simulation/render iteration slips through.
	if (!s_calypsoMainLoopPaused)
	{
		s_calypsoMainLoopPaused = 1;
		emscripten_pause_main_loop();
	}
	return 1;
}

int calypso_viewport_input_blocked(void)
{
	return s_calypsoViewportBlocked;
}

EMSCRIPTEN_KEEPALIVE
void calypso_set_viewport_supported(int supported)
{
	const int blocked = supported ? 0 : 1;
	const bool changed = blocked != s_calypsoViewportBlocked;
	s_calypsoViewportBlocked = blocked;
	// Always record the latest gate and drop held/stale browser input. During
	// callMain startup the rAF loop may not exist yet, so pause/resume is deferred
	// until the first registered callback proves MainLoop.func exists.
	SDL_FlushEvents(SDL_KEYDOWN, SDL_MULTIGESTURE);
	SDL_ResetKeyboard();
	if (!changed || !s_calypsoMainLoopStarted) return;
	if (blocked)
	{
		if (!s_calypsoMainLoopPaused)
		{
			s_calypsoMainLoopPaused = 1;
			emscripten_pause_main_loop();
		}
	}
	else if (!g_calypsoContextLost && s_calypsoMainLoopPaused)
	{
		s_calypsoMainLoopPaused = 0;
		calypso_restart_main_loop();
	}
}

/* M6c: WebGL context-loss / restore freeze.
 *
 * g_calypsoContextLost is tested at the top of Screen::flip() (and any other
 * per-frame GL entry) so the engine skips ALL GL calls while the context is
 * dead.  JS sets this flag synchronously on the 'webglcontextlost' event
 * (before the browser discards the GL objects) and clears it on restore.
 *
 * Emscripten's main loop is paused so the event / timer callbacks that drive
 * the game loop stop firing; only the SDL event queue (which is safe on a dead
 * context) and the two canvas event listeners continue to run.
 *
 * Edge cases handled:
 *   • Double-loss  — guard in calypso_gl_context_lost prevents double-pause.
 *   • Restore without prior loss — SDL_RENDER_TARGETS_RESET is still pushed.
 *   • Loss before Game::run — the flag is recorded without touching a missing
 *     loop; the first registered callback pauses and returns before iterate(). */
int g_calypsoContextLost = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_gl_context_lost(void)
{
	if (!g_calypsoContextLost)
	{
		g_calypsoContextLost = 1;
		if (s_calypsoMainLoopStarted && !s_calypsoMainLoopPaused)
		{
			s_calypsoMainLoopPaused = 1;
			emscripten_pause_main_loop();
		}
	}
}

EMSCRIPTEN_KEEPALIVE
void calypso_gl_context_restored(void)
{
	/* Do not reopen presentation here.  The replacement renderer/context and
	 * every registered GPU resource must pass Screen's transaction first. */
	g_calypsoContextLost = 1;
	s_calypsoRecoveryPending = 1;
	s_calypsoResetSentinelPending = 0;
	s_calypsoResetBoundaryOpen = 1;

	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_RENDER_TARGETS_RESET;
	SDL_PushEvent(&e);

	/* Recovery owns one bounded callback even while the viewport is blocked.
	 * The callback consumes SDL_RENDER_TARGETS_RESET and may commit GPU state,
	 * but calypso_pause_main_loop_before_iterate() keeps normal simulation out
	 * of that tick. Presentation remains closed until the viewport gate opens. */
	if (s_calypsoMainLoopStarted)
	{
		if (!s_calypsoMainLoopPaused)
		{
			s_calypsoMainLoopPaused = 1;
			emscripten_pause_main_loop();
		}
		calypso_restart_main_loop(); // contract-literal
		s_calypsoMainLoopPaused = 0;
	}
}

void calypso_context_recovery_succeeded(void)
{
	s_calypsoRecoveryPending = 0;
	/* The recovery callback may still be inside the old scheduler turn.  Let
	 * one fresh callback establish the new scheduler before set_main_loop_timing. */
	s_calypsoTimingReady = 0;
	g_calypsoContextLost = 0;
	if (s_calypsoMainLoopStarted && s_calypsoMainLoopPaused && !s_calypsoViewportBlocked)
		s_calypsoMainLoopPaused = 0;
}

void calypso_context_recovery_failed(void)
{
	s_calypsoRecoveryPending = 0;
	s_calypsoResetSentinelPending = 0;
	s_calypsoResetBoundaryOpen = 0;
	g_calypsoContextLost = 1;
	if (s_calypsoMainLoopStarted && !s_calypsoMainLoopPaused)
	{
		s_calypsoMainLoopPaused = 1;
		emscripten_pause_main_loop();
	}
}

int calypso_main_loop_timing_ready(void)
{
	return s_calypsoTimingReady && s_calypsoMainLoopStarted && !s_calypsoMainLoopPaused
		&& !g_calypsoContextLost && !s_calypsoRecoveryPending;
}

int calypso_context_reset_sentinel_pending(void)
{
	return s_calypsoResetSentinelPending;
}

void calypso_context_reset_sentinel_observed(void)
{
	/* Stage 10.2.7: transfer one-shot ownership only. The bounded window stays
	 * open across the reset transaction and into the first presented chain;
	 * closure belongs exclusively to the explicit boundary_close() owners
	 * (an owned-token consume or the end-of-chain close in Screen::flip). */
	if (s_calypsoResetBoundaryOpen)
	{
		s_calypsoResetSentinelPending = 1;
	}
}

int calypso_context_reset_boundary_open(void)
{
	return s_calypsoResetBoundaryOpen;
}

void calypso_context_reset_boundary_close(void)
{
	s_calypsoResetBoundaryOpen = 0;
}

void calypso_context_reset_sentinel_consumed(void)
{
	s_calypsoResetSentinelPending = 0;
}

} /* extern "C" */

#endif /* __EMSCRIPTEN__ */
