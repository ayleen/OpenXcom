#pragma once

#include <cstdint>

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoViewportScene
{
	Strategic,
	Tactical
};

struct CalypsoViewportRootSeed
{
	int width = 0;
	int height = 0;
};

/// Return the geometry a newly constructed explicit scene root actually used.
/// Tactical roots consume the active framebuffer. GeoscapeState constructs its
/// surfaces from the independently stored strategic base even when created
/// while Battlescape remains the active framebuffer.
inline CalypsoViewportRootSeed calypsoViewportRootSeed(
	CalypsoViewportScene scene,
	int activeWidth, int activeHeight,
	int strategicWidth, int strategicHeight)
{
	return scene == CalypsoViewportScene::Strategic
		? CalypsoViewportRootSeed{strategicWidth, strategicHeight}
		: CalypsoViewportRootSeed{activeWidth, activeHeight};
}

struct CalypsoViewportGeometry
{
	int width = 0;
	int height = 0;
	std::uint64_t generation = 0;
	bool valid = false;

	bool matches(const CalypsoViewportGeometry& other) const
	{
		return valid && other.valid && width == other.width && height == other.height
		    && generation == other.generation;
	}
};

/// Tracks the framebuffer geometry each persistent scene root has actually
/// consumed. Desired geometry may advance while that root is hidden; retaining
/// the applied geometry lets Game restore it before calling the real resize()
/// override when the root becomes visible again.
class CalypsoSceneViewportTracker
{
public:
	void observeRoot(CalypsoViewportScene scene, const void *root,
	                 int appliedWidth, int appliedHeight, std::uint64_t generation)
	{
		Slot& value = slot(scene);
		if (value.root == root) return;
		value.root = root;
		value.applied = root
			? CalypsoViewportGeometry{appliedWidth, appliedHeight, generation, true}
			: CalypsoViewportGeometry{};
	}

	void setDesired(CalypsoViewportScene scene, int width, int height,
	                std::uint64_t generation)
	{
		slot(scene).desired = CalypsoViewportGeometry{width, height, generation, true};
	}

	void markApplied(CalypsoViewportScene scene, int width, int height,
	                 std::uint64_t generation)
	{
		Slot& value = slot(scene);
		if (value.root)
			value.applied = CalypsoViewportGeometry{width, height, generation, true};
	}

	/// Accept a scene-owned geometry change performed outside the viewport
	/// reflow path (currently Battlescape zoom). Both sides advance together so
	/// a later overlay init cannot replay the old delta. A different root is
	/// rejected rather than allowing a stale callback to overwrite its state.
	bool acceptOutOfBandApplied(CalypsoViewportScene scene, const void *root,
	                           int width, int height, std::uint64_t generation)
	{
		Slot& value = slot(scene);
		if (!root || value.root != root) return false;
		const CalypsoViewportGeometry geometry{width, height, generation, true};
		value.desired = geometry;
		value.applied = geometry;
		return true;
	}

	const void *root(CalypsoViewportScene scene) const { return slot(scene).root; }
	CalypsoViewportGeometry desired(CalypsoViewportScene scene) const { return slot(scene).desired; }
	CalypsoViewportGeometry applied(CalypsoViewportScene scene) const { return slot(scene).applied; }
	bool needsCatchUp(CalypsoViewportScene scene) const
	{
		const Slot& value = slot(scene);
		return value.root && value.desired.valid && !value.applied.matches(value.desired);
	}

private:
	struct Slot
	{
		const void *root = nullptr;
		CalypsoViewportGeometry desired;
		CalypsoViewportGeometry applied;
	};

	Slot& slot(CalypsoViewportScene scene)
	{
		return scene == CalypsoViewportScene::Tactical ? _tactical : _strategic;
	}
	const Slot& slot(CalypsoViewportScene scene) const
	{
		return scene == CalypsoViewportScene::Tactical ? _tactical : _strategic;
	}

	Slot _strategic;
	Slot _tactical;
};

} // namespace Calypso
} // namespace OpenXcom
