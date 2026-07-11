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

inline bool hasProvenFilteredFallback(bool failureMemoryEnabled, bool candidateWasFiltered,
	AIFailureReason reason)
{
	return failureMemoryEnabled && candidateWasFiltered && reason != AIFailureReason::NONE;
}

inline AIFailureReason doorFailureReason(int doorResult)
{
	if (doorResult == 4) return AIFailureReason::DOOR_TU;
	if (doorResult == 5) return AIFailureReason::DOOR_RESERVED;
	return AIFailureReason::NONE;
}

inline int preserveActivationCounterForFallback(int counter, bool hasFilteredFallback)
{
	return hasFilteredFallback && counter < 1 ? 1 : counter;
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
	std::size_t size() const { return _attempts.size(); }
};

}
