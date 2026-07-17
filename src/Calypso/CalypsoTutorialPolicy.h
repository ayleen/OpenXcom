#pragma once

namespace OpenXcom
{
namespace Calypso
{

// Keep the two related gates explicit.  A scripted prologue suspends the
// generic campaign queue, but its own instructional beats still reflect the
// New Game choice stored in configuredEnabled.
inline bool genericTutorialEnabled(bool globallyEnabled, bool campaignEnabled)
{
	return globallyEnabled && campaignEnabled;
}

inline bool prologueGuidanceEnabled(bool globallyEnabled, bool configuredEnabled)
{
	return globallyEnabled && configuredEnabled;
}

// The New Game choice is per campaign.  A previous campaign may have
// accepted or declined the prologue, but that history must not silently turn
// the checked tutorial control into a no-op for the next campaign.
inline bool prologueMissionEnabled(bool tutorialEnabled, bool deploymentAvailable)
{
	return tutorialEnabled && deploymentAvailable;
}

// Before configuredEnabled existed, prologue autosaves persisted
// enabled=false only because generic tutorial UI was temporarily suspended.
// Preserve genuine legacy campaign opt-outs, but migrate that throwaway-save
// state back to the default-on New Game policy.
inline bool configuredTutorialFromSave(bool serializedEnabled, bool hasConfiguredValue,
	bool serializedConfiguredValue, bool legacyPrologueSave)
{
	if (hasConfiguredValue) return serializedConfiguredValue;
	return legacyPrologueSave ? true : serializedEnabled;
}

} // namespace Calypso
} // namespace OpenXcom
