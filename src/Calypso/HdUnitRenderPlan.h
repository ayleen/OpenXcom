#pragma once
/*
 * HdUnitRenderPlan.h -- dependency-free helpers for the HD-unit GPU draw plan.
 *
 * Keep the batching rule here testable without an OpenGL context: painter order
 * is established by the caller, then only adjacent compatible records may be
 * coalesced.  Non-adjacent records must remain separate or translucent RGBA
 * edges would composite in a different order.
 */
#include <algorithm>
#include <cstddef>
#include <cstdint>
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

/// Normalised sub-rectangle of a unit quad. A zero-sized result is invisible;
/// (0,0,1,1) is the unmodified quad. The GPU vertex shaders use the same record
/// for the colour and depth replays, so a walking unit cannot regain pixels in
/// the depth prepass that were removed by GraphSubset on the CPU path.
struct QuadClip
{
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
	bool visible = false;
	bool clipped = false;
};

inline QuadClip clipQuad(float screenX, float screenY, float quadW, float quadH,
	                     int maskBegX, int maskEndX, int maskBegY, int maskEndY)
{
	QuadClip result;
	if (!(quadW > 0.0f) || !(quadH > 0.0f)
	 || maskEndX <= maskBegX || maskEndY <= maskBegY)
		return result;

	const float left = std::max(screenX, (float)maskBegX);
	const float top = std::max(screenY, (float)maskBegY);
	const float right = std::min(screenX + quadW, (float)maskEndX);
	const float bottom = std::min(screenY + quadH, (float)maskEndY);
	if (!(right > left) || !(bottom > top))
		return result;

	result.x = (left - screenX) / quadW;
	result.y = (top - screenY) / quadH;
	result.w = (right - left) / quadW;
	result.h = (bottom - top) / quadH;
	result.visible = true;
	result.clipped = result.x > 0.0f || result.y > 0.0f
	              || result.w < 1.0f || result.h < 1.0f;
	return result;
}

struct Rgba8
{
	std::uint8_t r = 0, g = 0, b = 0, a = 0;
};

/// Integer source-over reference used by native regression tests for the
/// fractional-alpha unit painter. It intentionally matches the renderer's
/// straight-alpha blend state (SRC_ALPHA, ONE_MINUS_SRC_ALPHA).
inline Rgba8 sourceOver(Rgba8 src, Rgba8 dst)
{
	auto channel = [src](unsigned s, unsigned d) -> std::uint8_t {
		return (std::uint8_t)((s * src.a + d * (255u - src.a) + 127u) / 255u);
	};
	return {channel(src.r, dst.r), channel(src.g, dst.g), channel(src.b, dst.b),
	        (std::uint8_t)(src.a + (dst.a * (255u - src.a) + 127u) / 255u)};
}

/// The GL path maps larger iso to smaller depth and uses GL_LESS. This helper
/// documents/tests the foreground-occlusion relation without needing a context.
inline bool foregroundOccludes(float foregroundIso, float unitIso)
{
	return foregroundIso > unitIso;
}

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
