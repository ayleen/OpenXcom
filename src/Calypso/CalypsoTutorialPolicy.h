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

} // namespace Calypso
} // namespace OpenXcom
