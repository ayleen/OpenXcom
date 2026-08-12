#pragma once
/*
 * Phase 44 (Calypso) -- pure voice-bark arbitration helper, dependency-free so
 * the native doctest suite can exercise the real decision table. The browser
 * voice pilot (CalypsoVoiceG05 / CalypsoVoiceManager) delegates here.
 *
 * Hard rule: no engine, SDL, YAML, or OXCE includes may ever be added to this
 * header. Inputs are modelled by value; the function is state-free and total.
 */
#include <cstdint>

namespace OpenXcom
{

/// Global minimum spacing between any two barks, in milliseconds.
constexpr std::uint32_t CALYPSO_VOICE_GLOBAL_GAP_MS = 800u;

/// Minimum spacing between two barks from the same speaker, in milliseconds.
constexpr std::uint32_t CALYPSO_VOICE_SPEAKER_GAP_MS = 1500u;

/// Outcome of arbitrating one submitted voice event.
enum class CalypsoVoiceDecision
{
	PlayNow,       ///< Start playback immediately (free channel, interrupt, or force).
	Queue,         ///< No pending event; defer until the channel frees.
	ReplaceQueued, ///< Safety event preempts a lower-priority pending event.
	Suppress       ///< Drop: cooldown, gap fail with a full queue, or lost priority.
};

/// Final ownership decision for events that also have a one-shot stock sound.
/// A custom request must not be deferred or silently suppressed after the
/// caller has given up that stock response. Only immediate custom playback may
/// claim ownership; every other arbitration result returns the stock path.
struct CalypsoVoiceStockOwnership
{
	CalypsoVoiceDecision decision = CalypsoVoiceDecision::Suppress;
	bool allowStockFallback = false;
};

inline CalypsoVoiceStockOwnership calypsoResolveVoiceStockOwnership(
	CalypsoVoiceDecision decision, bool stockResponseExists)
{
	if (stockResponseExists && decision != CalypsoVoiceDecision::PlayNow)
	{
		return {CalypsoVoiceDecision::Suppress, true};
	}
	return {decision, false};
}

/// Wrap-safe elapsed milliseconds from `last` to `now`. The subtraction is
/// modulo 2^32, so a counter that wraps past 0 still yields the correct delta
/// for any real elapsed window shorter than ~49.7 days (2^32 ms).
inline std::uint32_t calypsoVoiceElapsedMs(std::uint32_t now, std::uint32_t last)
{
	return now - last;
}

/// Everything the arbiter needs to know about one submitted event. Every field
/// defaults to a "no prior state" value so tests can override only what matters.
struct CalypsoVoiceEventSubmission
{
	std::uint32_t nowMs = 0;

	/// Last time any bark fired, global clock. `hasLastGlobalMs == false` on
	/// the very first bark of a mission (gap is then considered satisfied).
	bool hasLastGlobalMs = false;
	std::uint32_t lastGlobalMs = 0;

	/// Last time this speaker fired a bark. `hasLastSpeakerMs == false` skips
	/// the per-speaker gap (e.g. first bark from this unit).
	bool hasLastSpeakerMs = false;
	std::uint32_t lastSpeakerMs = 0;

	/// Last time this exact event (unit+event key) fired. `hasLastEventMs ==
	/// false` skips the per-event cooldown check.
	bool hasLastEventMs = false;
	std::uint32_t lastEventMs = 0;

	/// Per-event cooldown supplied by the caller (ruleset-derived). 0 means
	/// "this event never cools down".
	std::uint32_t eventCooldownMs = 0;

	/// True when the voice channel is currently playing a clip.
	bool channelPlaying = false;
	/// Priority of the clip currently occupying the channel (valid only when
	/// `channelPlaying`).
	int currentPriority = 0;

	/// True when one event is already queued (at most one pending slot).
	bool pendingPresent = false;
	/// Priority of the pending event (valid only when `pendingPresent`).
	int pendingPriority = 0;

	/// Priority the caller is requesting for this submission.
	int requestedPriority = 0;

	/// Flavor events (selection / reselection / idle annoyance) never
	/// interrupt playing audio; they may still queue.
	bool isFlavor = false;

	/// Safety events (friendly_hit / civilian_hit) may preempt a queued
	/// lower-priority event when the single pending slot is occupied.
	bool isSafety = false;

	/// Force events bypass every gate and play immediately.
	bool isForce = false;
};

/// Deterministic, state-free arbitration for one submitted voice event.
///
/// Decision order:
///   1. Force bypasses everything.
///   2. Per-event cooldown failure drops the event (queueing would defeat it).
///   3. Global + per-speaker gaps gate both "free channel start" and
///      "interrupt playing audio".
///   4. Free channel + gaps elapsed -> play now.
///   5. Busy channel + gaps elapsed + non-flavor + strictly higher priority
///      -> interrupt (play now).
///   6. Otherwise the event cannot start; if the pending slot is empty it is
///      queued, else a safety event may replace a strictly lower-priority
///      pending event, else the event is suppressed.
inline CalypsoVoiceDecision calypsoDecideVoiceEvent(
	const CalypsoVoiceEventSubmission &s)
{
	if (s.isForce)
	{
		return CalypsoVoiceDecision::PlayNow;
	}

	if (s.hasLastEventMs
		&& calypsoVoiceElapsedMs(s.nowMs, s.lastEventMs) < s.eventCooldownMs)
	{
		return CalypsoVoiceDecision::Suppress;
	}

	const bool globalGapOk = !s.hasLastGlobalMs
		|| calypsoVoiceElapsedMs(s.nowMs, s.lastGlobalMs) >= CALYPSO_VOICE_GLOBAL_GAP_MS;
	const bool speakerGapOk = !s.hasLastSpeakerMs
		|| calypsoVoiceElapsedMs(s.nowMs, s.lastSpeakerMs) >= CALYPSO_VOICE_SPEAKER_GAP_MS;
	const bool gapOk = globalGapOk && speakerGapOk;

	if (!s.channelPlaying && gapOk)
	{
		return CalypsoVoiceDecision::PlayNow;
	}

	if (s.channelPlaying && gapOk && !s.isFlavor
		&& s.requestedPriority > s.currentPriority)
	{
		return CalypsoVoiceDecision::PlayNow;
	}

	if (!s.pendingPresent)
	{
		return CalypsoVoiceDecision::Queue;
	}

	if (s.isSafety && s.requestedPriority > s.pendingPriority)
	{
		return CalypsoVoiceDecision::ReplaceQueued;
	}

	return CalypsoVoiceDecision::Suppress;
}

}
