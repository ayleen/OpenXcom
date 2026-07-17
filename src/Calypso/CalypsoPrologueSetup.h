#pragma once

/*
 * Phase 41 (Calypso) -- immutable setup values for the scripted prologue.
 * Kept dependency-free so the native unit-test target can guard the intended
 * scenario tuning without needing Emscripten or Battlescape objects.
 */

namespace OpenXcom
{
namespace Calypso
{

// The Accord Assessor must keep pace with the expedition's 50-54 TU divers.
constexpr int PROLOGUE_ASSESSOR_TIME_UNITS = 54;

inline bool prologueAssessorTimeUnitsNeedRepair(int serializedMaximum)
{
	return serializedMaximum != PROLOGUE_ASSESSOR_TIME_UNITS;
}

inline bool prologueOfficeMarkerVisible(bool inert, bool endingTriggered, bool moveToOfficePhase)
{
	return !inert && !endingTriggered && moveToOfficePhase;
}

} // namespace Calypso
} // namespace OpenXcom
