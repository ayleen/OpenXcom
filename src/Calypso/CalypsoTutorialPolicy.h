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
