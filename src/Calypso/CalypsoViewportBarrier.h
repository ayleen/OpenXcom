#pragma once

#include <array>
#include <cstddef>

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoViewportBarrierStep
{
	FlushPending,
	SyncScene
};

struct CalypsoViewportBarrierPlan
{
	std::array<CalypsoViewportBarrierStep, 2> steps{};
	std::size_t count = 0;
};

/// The last barrier before State::init must consume a queued physical change
/// before calculating scene catch-up from the retained applied geometry.
inline CalypsoViewportBarrierPlan calypsoViewportBarrierPlan(bool hasPending)
{
	CalypsoViewportBarrierPlan plan;
	if (hasPending) plan.steps[plan.count++] = CalypsoViewportBarrierStep::FlushPending;
	plan.steps[plan.count++] = CalypsoViewportBarrierStep::SyncScene;
	return plan;
}

template<typename FlushPending, typename SyncScene, typename Initialize>
inline void calypsoRunPreInitViewportBarrier(bool hasPending,
	FlushPending&& flushPending, SyncScene&& syncScene, Initialize&& initialize)
{
	const CalypsoViewportBarrierPlan plan = calypsoViewportBarrierPlan(hasPending);
	for (std::size_t i = 0; i < plan.count; ++i)
	{
		if (plan.steps[i] == CalypsoViewportBarrierStep::FlushPending)
			flushPending();
		else
			syncScene();
	}
	initialize();
}

} // namespace Calypso
} // namespace OpenXcom
