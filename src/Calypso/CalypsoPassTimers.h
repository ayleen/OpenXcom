#pragma once
/*
 * Phase 46.4 Option A (owner-approved scope extension) -- per-pass main-thread
 * cost probes for the Geoscape physical frame. Capability-gated: accumulation
 * happens only while calypsoPassTimersEnabled() is true (loopback QA token),
 * costing one predictable branch at each pass boundary. Read/reset never
 * mutate campaign, rendering, or input state.
 */
#include <cstdint>

namespace OpenXcom
{
namespace Calypso
{

/// Accumulated microseconds per owning pass, plus presented-frame count.
struct CalypsoGeoscapePassTimers
{
	std::uint64_t thinkUs;
	std::uint64_t blitUs;
	std::uint64_t flipUs;
	std::uint64_t earthUs;
	std::uint64_t borderUs;
	std::uint64_t radarUs;
	std::uint64_t labelUs;
	std::uint64_t markerUs;
	std::uint64_t frames;
	/* Option A composite/chrome attribution probes (appended so existing
	 * indexed readers stay stable). */
	std::uint64_t sdlTexUs;
	std::uint64_t sdlCopyUs;
	std::uint64_t chromeUs;
	/* Composite sub-split (Option A attribution round 3). */
	std::uint64_t sdlBlitUs;
	std::uint64_t sdlMemcpyUs;
};

inline bool& calypsoPassTimersEnabledRef()
{
	static bool enabled = false;
	return enabled;
}

inline bool calypsoPassTimersEnabled() { return calypsoPassTimersEnabledRef(); }
inline void calypsoSetPassTimersEnabled(bool on) { calypsoPassTimersEnabledRef() = on; }

inline CalypsoGeoscapePassTimers& calypsoPassTimers()
{
	static CalypsoGeoscapePassTimers timers = CalypsoGeoscapePassTimers();
	return timers;
}

} // namespace Calypso
} // namespace OpenXcom
