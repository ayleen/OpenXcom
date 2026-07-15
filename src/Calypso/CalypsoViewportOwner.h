#pragma once

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoViewportOwner
{
	StrategicRoot,
	StrategicRootless,
	TacticalRoot
};

/// Select the state that owns the active base-resolution delta. A SavedBattle
/// without an actual BattlescapeState is an initial/info Briefing path: it is
/// visually strategic, and GeoscapeState::resize deliberately refuses that
/// transition, so the bridge must apply the precomputed strategic target.
inline CalypsoViewportOwner calypsoViewportOwner(
	bool hasBattleRoot, bool hasGeoRoot, bool hasSavedBattle)
{
	if (hasBattleRoot) return CalypsoViewportOwner::TacticalRoot;
	if (hasGeoRoot && !hasSavedBattle) return CalypsoViewportOwner::StrategicRoot;
	return CalypsoViewportOwner::StrategicRootless;
}

} // namespace Calypso
} // namespace OpenXcom
