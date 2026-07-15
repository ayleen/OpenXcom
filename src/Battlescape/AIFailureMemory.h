#pragma once

#include "Position.h"
#include <cstdint>
#include <set>

namespace OpenXcom
{

enum class AIFailureReason : unsigned char
{
	NONE = 0, PATH_UNREACHABLE, PATH_BLOCKED, DOOR_TU, DOOR_RESERVED, TU_RESERVED,
	NO_LOF, NO_TRAJECTORY, INVALID_THROW, OUT_OF_RANGE, NO_AMMO,
	NOT_ENOUGH_TU, NOT_ENOUGH_ENERGY, INVALID_TARGET
};

inline bool isBoundedRetryEligible(bool failureMemoryEnabled, bool candidateEligible,
	AIFailureReason reason)
{
	// Phase 43 review fix #4: `candidateEligible` is the honest bounded-retry
	// eligibility flag (aiFailureMemoryCandidate) -- it means the candidate went through
	// candidateAllowed filtering, NOT that an alternate action exists. The callers
	// (BattlescapeGame) use it only to decide whether a single same-activation retry is
	// warranted; they must NOT treat it as proof of a viable alternate.
	return failureMemoryEnabled && candidateEligible && reason != AIFailureReason::NONE;
}

inline bool failureMemoryEndsActivation(bool failureMemoryEnabled, bool hasCurrentRevisionFailure)
{
	// Phase 43 review fix #4 (extracted terminal decision): when failure memory is enabled
	// AND a candidate already failed for the current world revision, a bounded same-activation
	// retry produced no committable action, so the activation must terminate (BA_NONE) instead
	// of looping forever on BA_RETHINK. Disabled memory or no current-revision failure preserves
	// the legacy BA_RETHINK behaviour. Semantics are identical to the previously open-coded boolean.
	return failureMemoryEnabled && hasCurrentRevisionFailure;
}

inline AIFailureReason doorFailureReason(int doorResult)
{
	if (doorResult == 4) return AIFailureReason::DOOR_TU;
	if (doorResult == 5) return AIFailureReason::DOOR_RESERVED;
	return AIFailureReason::NONE;
}

inline int preserveActivationCounterForBoundedRetry(int counter, bool hasBoundedRetry)
{
	return hasBoundedRetry && counter < 1 ? 1 : counter;
}

inline bool isFirstBoundedRetry(bool failureMemoryEnabled, bool candidateEligible,
	AIFailureReason reason, bool hasCurrentRevisionFailure)
{
	// Phase 43 one-retry contract: a bounded same-activation retry (the activation-counter
	// preserve in BattlescapeGame and the popState C2 zero-TU bypass) is granted ONLY on the
	// FIRST eligible candidate failure at the current world revision. A later eligible failure
	// at the same revision must neither preserve nor bypass -- the activation terminates
	// normally (terminal BA_NONE / end-turn) instead of scheduling yet another retry (C).
	// Disabled memory OR an already-recorded current-revision failure degrades to the legacy
	// (non-retry) behaviour. `hasCurrentRevisionFailure` is the result of
	// AIFailureMemory::hasFailureForRevision(revision) captured BEFORE recordFailedAttempt,
	// so the attempt we are about to record cannot retro-claim the "first" slot.
	return isBoundedRetryEligible(failureMemoryEnabled, candidateEligible, reason)
		&& !hasCurrentRevisionFailure;
}

inline bool beginsNewActivationAfterIncrement(int counter)
{
	return counter == 1;
}

struct AIFailedAttempt
{
	int action = 0;
	int targetId = -1;
	Position position;
	AIFailureReason reason = AIFailureReason::NONE;
	std::uint64_t worldRevision = 0;
};

struct AIFailedAttemptLess
{
	bool operator()(const AIFailedAttempt& a, const AIFailedAttempt& b) const
	{
		if (a.action != b.action) return a.action < b.action;
		if (a.targetId != b.targetId) return a.targetId < b.targetId;
		if (a.position.x != b.position.x) return a.position.x < b.position.x;
		if (a.position.y != b.position.y) return a.position.y < b.position.y;
		if (a.position.z != b.position.z) return a.position.z < b.position.z;
		if (a.reason != b.reason) return a.reason < b.reason;
		return a.worldRevision < b.worldRevision;
	}
};

class AIFailureMemory
{
	std::set<AIFailedAttempt, AIFailedAttemptLess> _attempts;
public:
	void clear() { _attempts.clear(); }
	void record(const AIFailedAttempt& attempt)
	{
		if (attempt.reason != AIFailureReason::NONE) _attempts.insert(attempt);
	}
	bool blocks(int action, int targetId, const Position& position, std::uint64_t revision) const
	{
		for (const auto& failed : _attempts)
			if (failed.action == action && failed.targetId == targetId && failed.position == position
				&& failed.worldRevision == revision) return true;
		return false;
	}
	bool allows(int action, int targetId, const Position& position, std::uint64_t revision) const
	{
		return !blocks(action, targetId, position, revision);
	}
	/// True if any failure was recorded for `revision`. Used by the brutalThink
	/// no-progress hardlock (Phase 43 review fix #4): once a candidate for the current
	/// world revision has failed and the bounded retry produced no committable action,
	/// the activation must terminate (BA_NONE) instead of looping on BA_RETHINK.
	bool hasFailureForRevision(std::uint64_t revision) const
	{
		for (const auto& failed : _attempts)
			if (failed.worldRevision == revision) return true;
		return false;
	}
	std::size_t size() const { return _attempts.size(); }
};

}
