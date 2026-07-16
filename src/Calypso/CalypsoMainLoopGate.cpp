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
extern int g_calypsoContextLost;

void calypso_reset_main_loop_state(void)
{
	s_calypsoMainLoopStarted = 0;
	s_calypsoMainLoopPaused = 0;
}

int calypso_pause_main_loop_before_iterate(void)
{
	// Reaching the callback is the first authoritative proof that Emscripten has
	// installed MainLoop.func. A flag set before set_main_loop_arg is too early:
	// JS can run between those points and resume a still-null loop.
	s_calypsoMainLoopStarted = 1;
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
		emscripten_resume_main_loop();
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
	const int wasLost = g_calypsoContextLost;
	g_calypsoContextLost = 0;

	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_RENDER_TARGETS_RESET;
	SDL_PushEvent(&e);

	if (wasLost && s_calypsoMainLoopStarted && s_calypsoMainLoopPaused
	    && !s_calypsoViewportBlocked)
	{
		s_calypsoMainLoopPaused = 0;
		emscripten_resume_main_loop();
	}
}

} /* extern "C" */

#endif /* __EMSCRIPTEN__ */
