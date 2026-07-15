#ifdef __EMSCRIPTEN__
/*
 * Calypso viewport bridge (Phase 46.1) -- extracted verbatim from
 * Engine/EmscriptenHarness.cpp (policy R3 / R6 relocation-only).
 *
 * Owns the authoritative CSS-logical + canvas-physical viewport runtime
 * (CalypsoViewportRuntime holder, pending-resize transaction, canvas-fallback
 * notification) and the JS-facing EMSCRIPTEN_KEEPALIVE viewport query/notify
 * exports. extern "C" symbol names and ABI are preserved exactly; see
 * `git diff --color-moved=dimmed-zebra`.
 */
#include <emscripten.h>
#include <SDL.h>
#include <string>
#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/Logger.h"
#include "../Savegame/SavedGame.h"
#include "CalypsoViewportRuntime.h"

using namespace OpenXcom;

namespace OpenXcom
{
namespace Calypso
{
static CalypsoViewportRuntime s_viewportRuntime;
static CalypsoPendingViewportResize s_pendingViewport;
static bool s_hasPendingViewport = false;

CalypsoViewportRuntime& calypsoViewportRuntime()
{
	return s_viewportRuntime;
}

static CalypsoVisualContext currentViewportContext()
{
	Game *g = getCurrentGame();
	return (g && g->hasActiveBattlescapeRoot())
		? CalypsoVisualContext::Tactical
		: CalypsoVisualContext::Strategic;
}

static bool queueViewportEvent(const CalypsoViewportUpdate& change)
{
	if (!change.anyChanged() || s_viewportRuntime.physicalWidth() <= 0
	    || s_viewportRuntime.physicalHeight() <= 0 || !getCurrentGame())
		return false;

	const CalypsoPendingViewportResize previousPending = s_pendingViewport;
	const bool previouslyQueued = s_hasPendingViewport;
	s_pendingViewport.logicalChanged = change.logicalChanged
	                                || (previouslyQueued && previousPending.logicalChanged);
	s_pendingViewport.physicalChanged = change.physicalChanged
	                                 || (previouslyQueued && previousPending.physicalChanged);
	s_pendingViewport.hadPreviousLayout = previouslyQueued
		? previousPending.hadPreviousLayout : change.hadPreviousLayout;
	s_pendingViewport.previousMetrics = previouslyQueued
		? previousPending.previousMetrics : change.previousMetrics;
	s_pendingViewport.metrics = change.metrics;
	s_pendingViewport.previousLogicalWidth = previouslyQueued
		? previousPending.previousLogicalWidth : change.previousLogicalWidth;
	s_pendingViewport.previousLogicalHeight = previouslyQueued
		? previousPending.previousLogicalHeight : change.previousLogicalHeight;
	s_pendingViewport.logicalWidth = s_viewportRuntime.current().logicalWidth;
	s_pendingViewport.logicalHeight = s_viewportRuntime.current().logicalHeight;
	s_pendingViewport.previousPhysicalWidth = previouslyQueued
		? previousPending.previousPhysicalWidth : change.previousPhysicalWidth;
	s_pendingViewport.previousPhysicalHeight = previouslyQueued
		? previousPending.previousPhysicalHeight : change.previousPhysicalHeight;
	s_pendingViewport.physicalWidth = s_viewportRuntime.physicalWidth();
	s_pendingViewport.physicalHeight = s_viewportRuntime.physicalHeight();
	s_pendingViewport.generation = change.generation;
	s_hasPendingViewport = true;

	// An event for the same physical dimensions is already in SDL's queue.
	// Coalesce the newest logical/safe-area snapshot into its pending payload;
	// the queued event will consume that latest snapshot on this same thread.
	if (previouslyQueued
	    && previousPending.physicalWidth == s_pendingViewport.physicalWidth
	    && previousPending.physicalHeight == s_pendingViewport.physicalHeight)
		return true;

	SDL_Event event;
	SDL_zero(event);
	event.type = SDL_WINDOWEVENT;
	event.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
	event.window.data1 = s_pendingViewport.physicalWidth;
	event.window.data2 = s_pendingViewport.physicalHeight;
	if (SDL_PushEvent(&event) <= 0)
	{
		// Never leave an orphan transition that no SDL event can consume. If an
		// older event was already queued, restore its matching payload exactly.
		s_pendingViewport = previousPending;
		s_hasPendingViewport = previouslyQueued;
		Log(LOG_WARNING) << "[ui-resize] failed to queue viewport event: " << SDL_GetError();
		return false;
	}
	return true;
}

bool calypsoConsumePendingViewportResize(int physicalWidth, int physicalHeight,
	                                     CalypsoPendingViewportResize& out)
{
	if (!s_hasPendingViewport || physicalWidth != s_pendingViewport.physicalWidth
	    || physicalHeight != s_pendingViewport.physicalHeight)
		return false;
	out = s_pendingViewport;
	s_hasPendingViewport = false;
	return true;
}

bool calypsoNotifyCanvasFallback(int physicalWidth, int physicalHeight)
{
	// A normal JS notification may be queued but not consumed when flip() polls
	// the just-resized canvas. Treat it as handled instead of creating a second
	// resize transaction.
	if (s_hasPendingViewport && physicalWidth == s_pendingViewport.physicalWidth
	    && physicalHeight == s_pendingViewport.physicalHeight)
		return true;

	CalypsoSafeInsets insets;
	int logicalWidth = physicalWidth;
	int logicalHeight = physicalHeight;
	CalypsoVisualContext context = currentViewportContext();
	if (s_viewportRuntime.hasLayout())
	{
		const CalypsoLayoutMetrics& current = s_viewportRuntime.current();
		logicalWidth = current.logicalWidth;
		logicalHeight = current.logicalHeight;
		insets.top = current.safeY;
		insets.left = current.safeX;
		insets.right = current.logicalWidth - current.safeX - current.safeWidth;
		insets.bottom = current.logicalHeight - current.safeY - current.safeHeight;
	}
	const CalypsoViewportUpdate change = s_viewportRuntime.update(
		logicalWidth, logicalHeight, physicalWidth, physicalHeight, insets, context);
	return queueViewportEvent(change);
}

} // namespace Calypso
} // namespace OpenXcom

extern "C" {

/* Phase 46.1: authoritative CSS-logical + canvas-physical viewport bridge.
 * Safe-area insets use CSS logical pixels. The holder is updated before the
 * Game exists too; only a live Game receives a synthetic SIZE_CHANGED event. */
EMSCRIPTEN_KEEPALIVE
void calypso_notify_viewport(int logicalWidth, int logicalHeight,
	                         int physicalWidth, int physicalHeight,
	                         int top, int right, int bottom, int left)
{
	using namespace OpenXcom::Calypso;
	const CalypsoViewportUpdate change = calypsoViewportRuntime().update(
		logicalWidth, logicalHeight, physicalWidth, physicalHeight,
		CalypsoSafeInsets{top, right, bottom, left}, currentViewportContext());
	queueViewportEvent(change);
}

/* JS numbers exactly represent every integer generation reached in practical
 * sessions (well below 2^53). The underlying C++ counter remains uint64_t and
 * saturating; this read-only probe intentionally returns double for ccall. */
EMSCRIPTEN_KEEPALIVE double calypso_viewport_generation()
{
	return static_cast<double>(OpenXcom::Calypso::calypsoViewportRuntime().generation());
}
EMSCRIPTEN_KEEPALIVE int calypso_viewport_logical_width()
{
	const auto& runtime = OpenXcom::Calypso::calypsoViewportRuntime();
	return runtime.hasLayout() ? runtime.current().logicalWidth : 0;
}

EMSCRIPTEN_KEEPALIVE
const char *calypso_viewport_gate_json()
{
	static std::string out;
	Game *g = getCurrentGame();
	if (!g || !g->getLanguage()) { out = ""; return out.c_str(); }
	auto escaped = [](const std::string& in) {
		std::string value;
		for (unsigned char c : in)
		{
			switch (c)
			{
			case '\\': value += "\\\\"; break;
			case '"': value += "\\\""; break;
			case '\n': value += "\\n"; break;
			case '\r': value += "\\r"; break;
			case '\t': value += "\\t"; break;
			default: if (c >= 0x20) value += static_cast<char>(c); break;
			}
		}
		return value;
	};
	Language *lang = g->getLanguage();
	const char *ids[] = {
		"STR_CAL_HD_VIEWPORT_TITLE", "STR_CAL_HD_VIEWPORT_BODY",
		"STR_CAL_HD_ROTATE_TITLE", "STR_CAL_HD_ROTATE_BODY",
		"STR_CAL_HD_VIEWPORT_CURRENT", "STR_CAL_HD_VIEWPORT_REQUIRED"
	};
	std::string values[6];
	for (int i = 0; i < 6; ++i)
	{
		values[i] = std::string(lang->getString(ids[i]));
		// Language::getString returns the id itself until the owning mod's
		// extraStrings are loaded. Empty output asks JS to retain its safe English
		// fallback and retry instead of ever painting raw STR_* keys.
		if (values[i].empty() || values[i] == ids[i])
		{
			out.clear();
			return out.c_str();
		}
	}
	out = "{\"sizeTitle\":\"" + escaped(values[0])
	    + "\",\"sizeBody\":\"" + escaped(values[1])
	    + "\",\"rotateTitle\":\"" + escaped(values[2])
	    + "\",\"rotateBody\":\"" + escaped(values[3])
	    + "\",\"current\":\"" + escaped(values[4])
	    + "\",\"required\":\"" + escaped(values[5]) + "\"}";
	return out.c_str();
}
EMSCRIPTEN_KEEPALIVE int calypso_viewport_logical_height()
{
	const auto& runtime = OpenXcom::Calypso::calypsoViewportRuntime();
	return runtime.hasLayout() ? runtime.current().logicalHeight : 0;
}
EMSCRIPTEN_KEEPALIVE int calypso_viewport_safe_x()
{
	const auto& runtime = OpenXcom::Calypso::calypsoViewportRuntime();
	return runtime.hasLayout() ? runtime.current().safeX : 0;
}
EMSCRIPTEN_KEEPALIVE int calypso_viewport_safe_y()
{
	const auto& runtime = OpenXcom::Calypso::calypsoViewportRuntime();
	return runtime.hasLayout() ? runtime.current().safeY : 0;
}
EMSCRIPTEN_KEEPALIVE int calypso_viewport_safe_width()
{
	const auto& runtime = OpenXcom::Calypso::calypsoViewportRuntime();
	return runtime.hasLayout() ? runtime.current().safeWidth : 0;
}
EMSCRIPTEN_KEEPALIVE int calypso_viewport_safe_height()
{
	const auto& runtime = OpenXcom::Calypso::calypsoViewportRuntime();
	return runtime.hasLayout() ? runtime.current().safeHeight : 0;
}

} /* extern "C" */

#endif /* __EMSCRIPTEN__ */
