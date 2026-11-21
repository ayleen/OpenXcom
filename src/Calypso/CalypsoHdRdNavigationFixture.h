#pragma once

/*
 * RR3-002 N01-N07 navigation fixtures.  The fixture only arms diagnostics
 * and describes the native owner setup; HostState remains responsible for
 * constructing/registering the actual ResearchInfoState or
 * ManufactureInfoState and for restoring the save lease.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoStrategicNavigation.h"

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoHdRdNavigationProfile : int
{
	NewResearch = 350,
	ExistingResearch = 351,
	NewProduction = 352,
	ExistingProduction = 353,
	NestedNewResearch = 354,
	NestedExistingResearch = 355,
	NestedNewProduction = 356,
	NestedExistingProduction = 357,
	StaleRouteIdentity = 358,
	IdempotentFinalizer = 359,
};

struct CalypsoHdRdNavigationProfileSpec
{
	int id;
	const char *owner;
	bool nested;
	bool replacesSaveIdentity;
	bool duplicatesBoundary;
};

inline const CalypsoHdRdNavigationProfileSpec *
calypsoHdRdNavigationProfileSpec(int id)
{
	static const CalypsoHdRdNavigationProfileSpec specs[] = {
		{350, "new-research", false, false, false},
		{351, "existing-research", false, false, false},
		{352, "new-production", false, false, false},
		{353, "existing-production", false, false, false},
		{354, "new-research", true, false, false},
		{355, "existing-research", true, false, false},
		{356, "new-production", true, false, false},
		{357, "existing-production", true, false, false},
		{358, "existing-research", false, true, false},
		{359, "existing-production", false, false, true},
	};
	for (const auto &spec : specs)
		if (spec.id == id) return &spec;
	return nullptr;
}

inline bool calypsoHdRdNavigationProfileValid(int id)
{
	return calypsoHdRdNavigationProfileSpec(id) != nullptr;
}

/// Reuse the already audited owner factories for the navigation fixture. The
/// navigation profile selects lifecycle setup; it does not synthesize a
/// second production/research implementation.
inline int calypsoHdRdNavigationUnderlyingProfile(int id)
{
	switch (id)
	{
	case 350:
	case 354: return 1;   // new research preview
	case 351:
	case 355:
	case 358: return 6;   // existing research assignment
	case 352:
	case 356: return 107; // unstarted production
	case 353:
	case 357:
	case 359: return 109; // existing production + deferred options
	default: return 0;
	}
}

/// Arm observations for the next real native owner. This does not create a
/// state or mutate the save; the harness factory does that after this call.
inline bool calypsoHdRdNavigationPrepareProfile(int id)
{
	const auto *spec = calypsoHdRdNavigationProfileSpec(id);
	if (!spec) return false;
	calypsoHdNavigationReset(spec->owner);
	calypsoHdNavigationClearPendingRoute();
	return true;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
