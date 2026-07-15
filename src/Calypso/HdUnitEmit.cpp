/*
 * Calypso HD-unit GPU emit seam.
 *
 * All production emission math and the disposable spike instrumentation live
 * outside upstream UnitSprite.cpp. UnitSprite supplies plain render inputs and
 * retains its original CPU fallback whenever this helper returns false.
 */
#ifdef __EMSCRIPTEN__

#include "HdUnitEmit.h"
#include "HdUnitBattleSpike.h"
#include "HdUnitRenderPlan.h"
#include "../Battlescape/UnitSprite.h"
#include "../Engine/GpuTexture.h"

#include <algorithm>
#include <cmath>

namespace OpenXcom
{
namespace
{

constexpr float kSubprioBase        = 4.0f;
constexpr float kSubprioStep        = 0.25f;
// Every draw routine must consume at most eight ordered parts (sequence 0..7).
// If a routine is added or extended beyond that, later parts clamp to the same
// depth and can no longer express their relative layering in the depth prepass.
constexpr int   kSubprioMaxSequence = 7;
constexpr float kSubprioOverlayEps  = 0.125f;
constexpr double kDepth24Max        = 16777215.0;
static_assert(kSubprioBase > 3.0f, "unit emissions must remain above floor items");
static_assert(kSubprioBase + kSubprioMaxSequence * kSubprioStep
	          + kSubprioOverlayEps < 6.0f,
	          "unit emissions must remain below foreground objects");
static_assert((double)kSubprioOverlayEps / (double)HdUnitRenderPlan::kIsoDivisor
	          > 1.0 / kDepth24Max,
	          "baseline and overlay must occupy distinct 24-bit depth levels");

float boundedSubpriority(int sequence)
{
	if (sequence < 0) sequence = 0;
	if (sequence > kSubprioMaxSequence) sequence = kSubprioMaxSequence;
	return kSubprioBase + (float)sequence * kSubprioStep;
}

bool scalePartOffset(int logicalOffset, int scale, float& scaled)
{
	if (scale <= 0) return false;
	scaled = (float)logicalOffset * (float)scale;
	return std::isfinite(scaled);
}

void emitRgbaOverlay(const HdUnitAtlasSpec* spec, int frameIdx,
	                 const HdTileInstance& baseline, float basePriority,
	                 size_t baselineIndex,
	                 std::vector<std::vector<HdRgbaOverlayInstance>>* pages)
{
	if (!pages || !hdUnitRgbaPageUsable(spec, frameIdx)) return;
	const int page = spec->framePageOf(frameIdx);
	if (page < 0 || page >= (int)spec->rgbaOverlayPages.size()
	 || page >= (int)pages->size() || spec->rgbaFramesPerPage <= 0
	 || spec->rgbaColumns <= 0 || spec->rgbaPageW <= 0 || spec->rgbaPageH <= 0)
		return;
	const int idxInPage = frameIdx - page * spec->rgbaFramesPerPage;
	const int col = idxInPage % spec->rgbaColumns;
	const int row = idxInPage / spec->rgbaColumns;
	const float uvW = (float)spec->frameWidth / (float)spec->rgbaPageW;
	const float uvH = (float)spec->frameHeight / (float)spec->rgbaPageH;
	HdTileInstance overlay = baseline;
	overlay.atlasU = col * uvW;
	overlay.atlasV = row * uvH;
	overlay.iso = (basePriority + kSubprioOverlayEps) / HdUnitRenderPlan::kIsoDivisor;
	(*pages)[(size_t)page].push_back({overlay, baselineIndex});
}

} // namespace

bool hdUnitRgbaPageUsable(const HdUnitAtlasSpec* spec, int frameIdx)
{
	if (!spec || !spec->hasRgbaOverlay() || !spec->frameHasHd(frameIdx)) return false;
	const int page = spec->framePageOf(frameIdx);
	if (page < 0 || page >= (int)spec->rgbaOverlayPages.size()) return false;
	GpuTexture* texture = spec->rgbaOverlayPages[(size_t)page];
	// A failed context-restore upload leaves the registered object alive but
	// clears its GL name. The emitter must leave the baseline unmasked then.
	return texture && texture->isValid();
}

HdUnitScalePlan makeHdUnitScalePlan(const HdUnitAtlasSpec* bodySpec,
	                                const HdUnitAtlasSpec* itemSpec,
	                                int renderW, int renderH)
{
	HdUnitScalePlan result;
	result.itemSpec = itemSpec;
	if (!bodySpec) return result;
	result.partOffsetScale = bodySpec->partScaleForFrame(renderW, renderH);
	result.valid = result.partOffsetScale > 0
		&& (!itemSpec || itemSpec->partScaleForFrame(renderW, renderH) == result.partOffsetScale);
	return result;
}

void setHdUnitEmitTargets(HdUnitEmitState& state, const HdUnitEmitTargets& targets,
	                      int partOffsetScale)
{
	state.targets = targets;
	state.targets.partOffsetScale = partOffsetScale > 0 ? partOffsetScale : 1;
}

void clearHdUnitEmitTargets(HdUnitEmitState& state)
{
	state = {};
}

void advanceHdUnitEmitSequence(HdUnitEmitState& state, HdUnitPartKind kind)
{
	const HdUnitAtlasSpec* spec = kind == HdUnitPartKind::Item
		? state.targets.itemSpec : state.targets.bodySpec;
	std::vector<HdTileInstance>* target = kind == HdUnitPartKind::Item
		? state.targets.itemInstances : state.targets.bodyInstances;
	if (target && spec && spec->atlas) ++state.sequence;
}

bool emitHdUnitPart(HdUnitEmitState& state, HdUnitPartKind kind,
	                int frameIdx, int logicalOffX, int logicalOffY,
	                bool indexedSource, int screenX, int screenY, int shade,
	                int maskBegX, int maskEndX, int maskBegY, int maskEndY,
	                int unitId, int direction)
{
	const bool item = kind == HdUnitPartKind::Item;
	const HdUnitAtlasSpec* spec = item ? state.targets.itemSpec : state.targets.bodySpec;
	std::vector<HdTileInstance>* instances = item
		? state.targets.itemInstances : state.targets.bodyInstances;
	if (!instances || !spec || !spec->atlas || frameIdx < 0 || !indexedSource) return false;

	float offX = 0.0f, offY = 0.0f;
	if (!scalePartOffset(logicalOffX, state.targets.partOffsetScale, offX)
	 || !scalePartOffset(logicalOffY, state.targets.partOffsetScale, offY)) return false;
	if (spec->columns <= 0 || spec->atlasW <= 0 || spec->atlasH <= 0) return false;

	const int col = frameIdx % spec->columns;
	const int row = frameIdx / spec->columns;
	const float uvW = (float)spec->tileWidth / (float)spec->atlasW;
	const float uvH = (float)spec->tileHeight / (float)spec->atlasH;
	const int sequence = state.sequence;
	const float localPriority = boundedSubpriority(sequence);
	const float emittedX = (float)screenX + offX;
	const float emittedY = (float)screenY + offY;
	const HdUnitRenderPlan::QuadClip clip = HdUnitRenderPlan::clipQuad(
		emittedX, emittedY, (float)state.targets.renderWidth,
		(float)state.targets.renderHeight,
		maskBegX, maskEndX, maskBegY, maskEndY);
	if (!clip.visible)
	{
		// The CPU GraphSubset path consumes a fully clipped part without drawing.
		// Preserve routine ordering while keeping all GPU colour/depth lists empty.
		++state.sequence;
		return true;
	}
	const float priority = (float)(state.targets.emitZ * 65536
		+ state.targets.emitY * 1024 + state.targets.emitX * 8) + localPriority;
	HdTileInstance instance = {
		emittedX, emittedY,
		col * uvW, row * uvH, (float)shade, 1.0f, 1.0f,
		priority / HdUnitRenderPlan::kIsoDivisor,
		clip.x, clip.y, clip.w, clip.h
	};
	instances->push_back(instance);

	auto* rgbaPages = item ? state.targets.rgbaOverlayItemPages
	                       : state.targets.rgbaOverlayBodyPages;
	emitRgbaOverlay(spec, frameIdx, instance, priority, instances->size() - 1, rgbaPages);

	const bool g0 = HdUnitBattleSpike::active() && state.targets.bodySpec
		&& state.targets.bodySpec->g0OverlayAtlas;
	if (g0)
	{
		HdUnitBattleSpike::recordEmit(unitId, direction, sequence,
			item ? "HANDOB" : "body", frameIdx, localPriority, false);
		if (!item && state.targets.g0OverlayTarget
		 && (size_t)frameIdx < state.targets.bodySpec->g0OverlayMask.size()
		 && state.targets.bodySpec->g0OverlayMask[(size_t)frameIdx])
		{
			HdTileInstance overlay = instance;
			overlay.iso = (priority + kSubprioOverlayEps) / HdUnitRenderPlan::kIsoDivisor;
			state.targets.g0OverlayTarget->push_back(overlay);
			HdUnitBattleSpike::recordOverlayGeometry(unitId, frameIdx,
				priority + kSubprioOverlayEps, overlay.screenX, overlay.screenY,
				(float)state.targets.bodySpec->tileWidth,
				(float)state.targets.bodySpec->tileHeight);
			HdUnitBattleSpike::recordEmit(unitId, direction, sequence, "body",
				frameIdx, localPriority + kSubprioOverlayEps, true);
		}
	}
	++state.sequence;

	std::vector<int>* zTarget = item ? state.targets.zTargetItem : state.targets.zTargetBody;
	std::vector<int>* yTarget = item ? state.targets.yTargetItem : state.targets.yTargetBody;
	if (zTarget) zTarget->push_back(state.targets.emitZ);
	if (yTarget) yTarget->push_back(state.targets.emitY);
	return true;
}

float hdUnitDebugE1LocalPriority(int sequence, bool overlay)
{
	return boundedSubpriority(sequence) + (overlay ? kSubprioOverlayEps : 0.0f);
}

unsigned int hdUnitDebugE1DepthCode(int basePriority, int sequence, bool overlay)
{
	const float priority = (float)basePriority + hdUnitDebugE1LocalPriority(sequence, overlay);
	const double depth = 1.0 - (double)(priority / HdUnitRenderPlan::kIsoDivisor);
	if (depth <= 0.0) return 0u;
	if (depth >= 1.0) return 0xFFFFFFu;
	return (unsigned int)(depth * kDepth24Max + 0.5);
}

bool hdUnitDebugE1DepthProof()
{
	for (int z = 0; z <= 15; ++z)
	for (int y = 0; y <= 255; ++y)
	for (int x = 0; x <= 255; ++x)
	{
		const int base = z * 65536 + y * 1024 + x * 8;
		for (int sequence = 0; sequence <= kSubprioMaxSequence; ++sequence)
		{
			const unsigned int baseline = hdUnitDebugE1DepthCode(base, sequence, false);
			const unsigned int overlay = hdUnitDebugE1DepthCode(base, sequence, true);
			if (!(overlay < baseline)) return false;
			if (sequence < kSubprioMaxSequence
			 && !(hdUnitDebugE1DepthCode(base, sequence + 1, false) < overlay)) return false;
		}
	}
	return true;
}

unsigned int hdUnitDebugE1FractionalPixel(bool reverseBuckets)
{
	struct Layer { float iso; unsigned r, g, b, a; bool maskedR8; };
	Layer layers[4] = {
		{hdUnitDebugE1LocalPriority(1, false), 0u, 255u, 0u, 255u, true},
		{hdUnitDebugE1LocalPriority(1, true), 0u, 0u, 255u, 255u, false},
		{hdUnitDebugE1LocalPriority(2, false), 255u, 255u, 0u, 255u, true},
		{hdUnitDebugE1LocalPriority(2, true), 255u, 0u, 0u, 128u, false}
	};
	if (reverseBuckets)
	{
		std::swap(layers[0], layers[2]);
		std::swap(layers[1], layers[3]);
	}
	std::stable_sort(layers, layers + 4,
		[](const Layer& lhs, const Layer& rhs) { return lhs.iso < rhs.iso; });
	unsigned r = 0, g = 0, b = 0, a = 0;
	for (const Layer& src : layers)
	{
		if (src.maskedR8) continue;
		r = (src.r * src.a + r * (255u - src.a) + 127u) / 255u;
		g = (src.g * src.a + g * (255u - src.a) + 127u) / 255u;
		b = (src.b * src.a + b * (255u - src.a) + 127u) / 255u;
		a = src.a + (a * (255u - src.a) + 127u) / 255u;
	}
	return (r << 24) | (g << 16) | (b << 8) | a;
}

int hdUnitDebugE2ScaledOffset(int logicalOffset, int scale)
{
	float scaled = 0.0f;
	return scalePartOffset(logicalOffset, scale, scaled) ? (int)scaled : 0;
}

bool hdUnitDebugE2OffsetProof()
{
	const int logical[] = {-7, -6, -2, -1, 0, 1, 2, 5, 7, 22};
	for (int value : logical)
	{
		float scaled = 0.0f;
		if (!scalePartOffset(value, 4, scaled) || scaled != (float)(value * 4)) return false;
	}
	float ignored = 0.0f;
	return !scalePartOffset(1, 0, ignored);
}

float UnitSprite::debugE1LocalPriority(int sequence, bool overlay)
{
	return hdUnitDebugE1LocalPriority(sequence, overlay);
}

unsigned int UnitSprite::debugE1DepthCode(int basePriority, int sequence, bool overlay)
{
	return hdUnitDebugE1DepthCode(basePriority, sequence, overlay);
}

bool UnitSprite::debugE1DepthProof()
{
	return hdUnitDebugE1DepthProof();
}

unsigned int UnitSprite::debugE1FractionalPixel(bool reverseBuckets)
{
	return hdUnitDebugE1FractionalPixel(reverseBuckets);
}

int UnitSprite::debugE2ScaledOffset(int logicalOffset, int scale)
{
	return hdUnitDebugE2ScaledOffset(logicalOffset, scale);
}

bool UnitSprite::debugE2OffsetProof()
{
	return hdUnitDebugE2OffsetProof();
}

} // namespace OpenXcom

#endif
