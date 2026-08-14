#pragma once

#include <cstdint>

namespace OpenXcom
{

enum class CalypsoVoiceMissionContextChange
{
	NewMission,
	Reconstruction,
	StageTransition
};

/// Distinguishes a UI reconstruction from a real map-stage transition. Pointer
/// identity alone is insufficient because OXCE reuses SavedBattleGame across
/// multi-stage deployments.
inline CalypsoVoiceMissionContextChange calypsoVoiceMissionContextChange(
	bool active, const void *activeSave, unsigned int activeStageGeneration,
	const void *requestedSave, unsigned int requestedStageGeneration)
{
	if (!active || activeSave != requestedSave)
	{
		return CalypsoVoiceMissionContextChange::NewMission;
	}
	return activeStageGeneration == requestedStageGeneration
		? CalypsoVoiceMissionContextChange::Reconstruction
		: CalypsoVoiceMissionContextChange::StageTransition;
}

/// Clears only arbitration state that is scoped to a single tactical map.
/// Loaded audio and pack-availability state are intentionally absent from this
/// contract so a multi-stage deployment can reuse them without another fetch.
template<typename PendingState, typename SpeakerCooldowns,
	typename EventCooldowns>
inline void calypsoResetVoiceStageArbitration(PendingState &pending,
	bool &hasLastGlobal, std::uint32_t &lastGlobal,
	SpeakerCooldowns &lastSpeaker, EventCooldowns &lastEvent)
{
	pending = PendingState{};
	hasLastGlobal = false;
	lastGlobal = 0;
	lastSpeaker.clear();
	lastEvent.clear();
}

}
