#pragma once

#include <vector>

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoViewportAffinity
{
	Inherit,
	Strategic,
	Tactical
};

enum class CalypsoViewportOwner
{
	StrategicRoot,
	StrategicRootless,
	TacticalRoot
};

/// Resolve the visible viewport context from affinities in top-to-bottom
/// order. Ordinary overlays inherit the first explicit screen below them.
inline CalypsoViewportAffinity calypsoResolveViewportAffinity(
	const std::vector<CalypsoViewportAffinity>& topDown,
	CalypsoViewportAffinity fallback = CalypsoViewportAffinity::Strategic)
{
	for (CalypsoViewportAffinity affinity : topDown)
		if (affinity != CalypsoViewportAffinity::Inherit) return affinity;
	return fallback;
}

/// Select the state that owns the active base-resolution delta. A SavedBattle
/// without an actual BattlescapeState is an initial/info Briefing path: it is
/// visually strategic, and GeoscapeState::resize deliberately refuses that
/// transition, so the bridge must apply the precomputed strategic target.
inline CalypsoViewportOwner calypsoViewportOwner(
	CalypsoViewportAffinity affinity,
	bool ownsBattleRoot, bool ownsGeoRoot, bool hasSavedBattle)
{
	if (affinity == CalypsoViewportAffinity::Tactical && ownsBattleRoot)
		return CalypsoViewportOwner::TacticalRoot;
	if (ownsGeoRoot && !hasSavedBattle) return CalypsoViewportOwner::StrategicRoot;
	return CalypsoViewportOwner::StrategicRootless;
}

} // namespace Calypso
} // namespace OpenXcom
