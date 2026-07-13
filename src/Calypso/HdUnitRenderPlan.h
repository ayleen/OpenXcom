#pragma once
/*
 * HdUnitRenderPlan.h -- dependency-free helpers for the HD-unit GPU draw plan.
 *
 * Keep the batching rule here testable without an OpenGL context: painter order
 * is established by the caller, then only adjacent compatible records may be
 * coalesced.  Non-adjacent records must remain separate or translucent RGBA
 * edges would composite in a different order.
 */
#include <cstddef>
#include <vector>

namespace OpenXcom
{
namespace HdUnitRenderPlan
{
constexpr float kIsoDivisor = 2000000.0f;

struct Run
{
	std::size_t first = 0;
	std::size_t count = 0;
};

template<typename Record, typename Compatible>
std::vector<Run> consecutiveRuns(const std::vector<Record>& records,
	Compatible compatible)
{
	std::vector<Run> runs;
	if (records.empty())
		return runs;

	std::size_t first = 0;
	for (std::size_t i = 1; i < records.size(); ++i)
	{
		if (!compatible(records[i - 1], records[i]))
		{
			runs.push_back({first, i - first});
			first = i;
		}
	}
	runs.push_back({first, records.size() - first});
	return runs;
}
} // namespace HdUnitRenderPlan
} // namespace OpenXcom
