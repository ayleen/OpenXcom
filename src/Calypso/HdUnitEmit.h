#pragma once
/*
 * HdUnitEmit.h — Calypso-owned, typed GPU instance records and the typed
 * emit-target bundle handed to UnitSprite/ItemSprite.
 *
 * Phase 42 review #6: UnitSprite::blitBody/blitItem previously static_cast
 * `void*` into `Map::TileInstance` / `Map::UnitAtlasGroup::RgbaOverlayInstance`
 * relying on `friend class UnitSprite`. That made the mandated src/Calypso/
 * extraction non-mechanical. These types now live here as ordinary free structs
 * under a whole-file guard; Map.h exposes them via using-aliases
 * (`Map::TileInstance`, `Map::UnitAtlasGroup::RgbaOverlayInstance`) so existing
 * references compile unchanged, and UnitSprite takes TYPED pointers — no void*,
 * no Map friendship needed to name the instance types.
 */
#ifdef __EMSCRIPTEN__
#include <cstddef>
#include <vector>
#include "HdUnitAtlas.h"

namespace OpenXcom
{
/// Per-tile / per-unit GPU instance record submitted to the tile_atlas shader.
/// Relocated verbatim from Map::TileInstance (Phase 36 R6). The instance layout
/// (12 floats) is asserted in MapGl.cpp::initTileGL — do not reorder fields.
struct HdTileInstance
{
	float screenX, screenY;   // top-left of tile in screen pixels
	float atlasU,  atlasV;    // UV of primary frame top-left in atlas
	float shade;              // 0..15
	float animFrameCount;     // total anim frames (>=1)
	float alphaMask;          // MCD opacity flag (0 or 1)
	float iso;                // iso priority [0..1]; larger = closer to camera
	// Normalised source/geometry sub-rect. clipW/H <= 0 means the legacy full
	// quad (terrain records predate this field); unit emits always set it.
	float clipX = 0.0f, clipY = 0.0f, clipW = 0.0f, clipH = 0.0f;
};

/// One production RGBA overlay sibling instance: the overlay's own geometry
/// (copied from its R8 baseline with a per-page UV) plus the index of the
/// matching baseline entry, so the draw pass can attach the page as an HD mask
/// onto that baseline instance.
struct HdRgbaOverlayInstance
{
	HdTileInstance instance;
	size_t baselineIndex = 0; // matching entry in the group's instances[]
};

/// Typed bundle of emit targets handed to UnitSprite::setEmitMode (replacing the
/// former void* parameters). UnitSprite only pushes into these vectors / advances
/// `sequence`; it never names Map's private nested types. Calypso-owned so it can
/// be passed across the Map→UnitSprite seam without a circular header dependency.
struct HdUnitEmitTargets
{
	std::vector<HdTileInstance>* bodyInstances = nullptr;
	std::vector<HdTileInstance>* itemInstances = nullptr;
	const HdUnitAtlasSpec*      bodySpec       = nullptr;
	const HdUnitAtlasSpec*      itemSpec       = nullptr;
	int emitZ = 0;  // unit's tile Z — drives iso priority + Z-slice interleaving
	int emitY = 0;  // unit's tile Y
	int emitX = 0;  // unit's tile X
	std::vector<int>* zTargetBody = nullptr;  // receives one int per body emit
	std::vector<int>* zTargetItem = nullptr;  // receives one int per item emit
	std::vector<int>* yTargetBody = nullptr;
	std::vector<int>* yTargetItem = nullptr;
	// Disposable G0 spike target (only populated when CALYPSO_HD_UNIT_SPIKE is on
	// AND the harness activated a real-battle probe); null in production.
	std::vector<HdTileInstance>* g0OverlayTarget = nullptr;
	// Phase 42 E1: production RGBA overlay emit targets (per-page instance lists,
	// indexed in lockstep with HdUnitAtlasSpec::rgbaOverlayPages).
	std::vector<std::vector<HdRgbaOverlayInstance>>* rgbaOverlayBodyPages = nullptr;
	std::vector<std::vector<HdRgbaOverlayInstance>>* rgbaOverlayItemPages = nullptr;
	int partOffsetScale = 1;  // E2: source-PCK offset -> live unit-quad pixels
	int renderWidth = 0;      // live unit quad dimensions used by GraphSubset
	int renderHeight = 0;
};

/// Mutable state owned by UnitSprite while a draw routine emits GPU instances.
/// Keeping the targets and ordering counter together lets the implementation
/// live in src/Calypso without exposing Map internals or untyped pointers.
struct HdUnitEmitState
{
	HdUnitEmitTargets targets;
	int sequence = 0;
};

enum class HdUnitPartKind : unsigned char { Body, Item };

/// Validate the shared body/HANDOB source-pixel scale used by the live quad.
struct HdUnitScalePlan
{
	const HdUnitAtlasSpec* itemSpec = nullptr;
	int partOffsetScale = 0;
	bool valid = false;
};

HdUnitScalePlan makeHdUnitScalePlan(const HdUnitAtlasSpec* bodySpec,
	                                const HdUnitAtlasSpec* itemSpec,
	                                int renderW, int renderH);

/// True only when the authored frame's exact RGBA page has a live GL handle.
/// Context-loss recovery uses this to fall back to R8 page-by-page.
bool hdUnitRgbaPageUsable(const HdUnitAtlasSpec* spec, int frameIdx);

void setHdUnitEmitTargets(HdUnitEmitState& state, const HdUnitEmitTargets& targets,
	                      int partOffsetScale);
void clearHdUnitEmitTargets(HdUnitEmitState& state);
void advanceHdUnitEmitSequence(HdUnitEmitState& state, HdUnitPartKind kind);

/// Emit one indexed body/HANDOB part. Returns true only when the GPU path
/// consumed the part; false keeps the caller on the unchanged CPU blit path.
bool emitHdUnitPart(HdUnitEmitState& state, HdUnitPartKind kind,
	                int frameIdx, int logicalOffX, int logicalOffY,
	                bool indexedSource, int screenX, int screenY, int shade,
	                int maskBegX, int maskEndX, int maskBegY, int maskEndY,
	                int unitId, int direction);

float hdUnitDebugE1LocalPriority(int sequence, bool overlay);
unsigned int hdUnitDebugE1DepthCode(int basePriority, int sequence, bool overlay);
bool hdUnitDebugE1DepthProof();
unsigned int hdUnitDebugE1FractionalPixel(bool reverseBuckets);
int hdUnitDebugE2ScaledOffset(int logicalOffset, int scale);
bool hdUnitDebugE2OffsetProof();
} // namespace OpenXcom
#endif
