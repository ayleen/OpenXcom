#pragma once

#include <cstdint>

namespace OpenXcom
{

constexpr std::uint32_t CALYPSO_VOICE_SELECTION_RESET_MS = 8000u;

enum class CalypsoVoiceSelectionFlavor
{
	None,
	Selected,
	Reselected,
	Annoyed1,
	Annoyed2,
	Annoyed3
};

struct CalypsoVoiceSelectionState
{
	int unitId = -1;
	unsigned int clicks = 0;
	bool hasLastClick = false;
	std::uint32_t lastClickMs = 0;
};

inline CalypsoVoiceSelectionFlavor calypsoAdvanceVoiceSelection(
	CalypsoVoiceSelectionState &state, int unitId, bool sameUnit,
	std::uint32_t nowMs, bool locked)
{
	if (!sameUnit)
	{
		state.unitId = unitId;
		state.clicks = 0;
		state.hasLastClick = false;
		state.lastClickMs = 0;
		return locked ? CalypsoVoiceSelectionFlavor::None
			: CalypsoVoiceSelectionFlavor::Selected;
	}

	if (state.unitId != unitId || !state.hasLastClick
		|| nowMs - state.lastClickMs > CALYPSO_VOICE_SELECTION_RESET_MS)
	{
		state.unitId = unitId;
		state.clicks = 1;
	}
	else
	{
		++state.clicks;
	}
	state.hasLastClick = true;
	state.lastClickMs = nowMs;

	if (locked)
	{
		return CalypsoVoiceSelectionFlavor::None;
	}
	switch (state.clicks)
	{
		case 1: return CalypsoVoiceSelectionFlavor::Reselected;
		case 3: return CalypsoVoiceSelectionFlavor::Annoyed1;
		case 5: return CalypsoVoiceSelectionFlavor::Annoyed2;
		case 7: return CalypsoVoiceSelectionFlavor::Annoyed3;
		default: return CalypsoVoiceSelectionFlavor::None;
	}
}

}
