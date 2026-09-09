#pragma once
// F07 submarine roster/overview/pilots shared tabbed-management shell.
//
// Binds the four F07 generated tabbed contracts (full-viewport Forces and
// submarine compositions per the stage-1 boards, §6.1). Layout lookup is
// fail-closed; geometry plumbing (projection, named-rect search, list seam,
// touch floors) is archetype-shared in CalypsoTabbedManagementRenderer.h.
// Rendering stays with the shared tabbed collector, never here.
//
// Capability visibility is authoritative from native state every frame:
// Overview always; Weapons from the live mount slots; Crew/Equipment/Armor
// only while the native tab buttons are visible (native init hides all four
// when RuleCraft::getMaxUnitsLimit() == 0, so a Ketos-like zero-capacity
// craft paints Overview + Weapons only); Pilots only while the native Pilots
// button is visible (additionally requires RuleCraft::getPilots() > 0).
// A hidden tab's route stays closed: no placeholder row, no dead-end.
//
// Parked controls stay fully live input/behavior owners: only their blit is
// suppressed and only their hit area moves (visibility flags and handler
// bindings are never modified), so keyboard paths and native gates behave
// exactly as before while no vanilla pixel can show or intercept pointer.
// Bound controls sit on generated tab/toolbar/detail/action slots with the
// native handler untouched; painted labels are always live native text
// (or localized chrome keys where no native owner exists), never invented
// copy.
//
// The pilot Remove-All readout correction (§7.4 quirk, CraftPilotsState
// .cpp:211-220) lives in calypsoF07RemoveAllReadoutVisible below: the ONLY
// approved enablement change in F04–F08, and it changes no mutation. It is
// kept separate from layout so it can land as its own fix commit.
#ifdef __EMSCRIPTEN__
#include <string>
#include "../Engine/Surface.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoTabbedManagementRenderer.h"
#include "Generated/CalypsoF07Crafts.generated.h"
#include "Generated/CalypsoF07CraftInfo.generated.h"
#include "Generated/CalypsoF07PilotSelect.generated.h"
#include "Generated/CalypsoF07CraftPilots.generated.h"

namespace OpenXcom
{
namespace Calypso
{

inline bool f07HdWideLayout()
{
	return calypsoTabbedWideLayout();
}

inline const CalypsoF07CraftsGen::CalypsoF07CraftsGenLayout* f07CraftsLayout(bool wide)
{
	const auto* generated = CalypsoF07CraftsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F07 roster generated layout is missing");
	return generated;
}

inline const CalypsoF07CraftInfoGen::CalypsoF07CraftInfoGenLayout* f07CraftInfoLayout(bool wide)
{
	const auto* generated = CalypsoF07CraftInfoGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F07 overview generated layout is missing");
	return generated;
}

inline const CalypsoF07PilotSelectGen::CalypsoF07PilotSelectGenLayout* f07PilotSelectLayout(bool wide)
{
	const auto* generated = CalypsoF07PilotSelectGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F07 pilot selector generated layout is missing");
	return generated;
}

inline const CalypsoF07CraftPilotsGen::CalypsoF07CraftPilotsGenLayout* f07CraftPilotsLayout(bool wide)
{
	const auto* generated = CalypsoF07CraftPilotsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F07 pilots generated layout is missing");
	return generated;
}

/// Generated slot rect by stable id (adapters never invent geometry).
inline CalypsoLogicalRect f07CraftsTabRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF07CraftsGen::kTabRects[wide ? 0 : 1],
		CalypsoF07CraftsGen::kTabCount, id);
}

inline CalypsoLogicalRect f07CraftsSummaryRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF07CraftsGen::kSummaryRects[wide ? 0 : 1],
		CalypsoF07CraftsGen::kSummaryCount, id);
}

inline CalypsoLogicalRect f07CraftsActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF07CraftsGen::kActionRects[wide ? 0 : 1],
		CalypsoF07CraftsGen::kActionCount, id);
}

inline CalypsoLogicalRect f07CraftInfoTabRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF07CraftInfoGen::kTabRects[wide ? 0 : 1],
		CalypsoF07CraftInfoGen::kTabCount, id);
}

inline CalypsoLogicalRect f07CraftInfoSummaryRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF07CraftInfoGen::kSummaryRects[wide ? 0 : 1],
		CalypsoF07CraftInfoGen::kSummaryCount, id);
}

inline CalypsoLogicalRect f07CraftInfoActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF07CraftInfoGen::kActionRects[wide ? 0 : 1],
		CalypsoF07CraftInfoGen::kActionCount, id);
}

inline CalypsoLogicalRect f07PilotSelectTabRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF07PilotSelectGen::kTabRects[wide ? 0 : 1],
		CalypsoF07PilotSelectGen::kTabCount, id);
}

inline CalypsoLogicalRect f07PilotSelectActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF07PilotSelectGen::kActionRects[wide ? 0 : 1],
		CalypsoF07PilotSelectGen::kActionCount, id);
}

inline CalypsoLogicalRect f07CraftPilotsTabRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF07CraftPilotsGen::kTabRects[wide ? 0 : 1],
		CalypsoF07CraftPilotsGen::kTabCount, id);
}

inline CalypsoLogicalRect f07CraftPilotsActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF07CraftPilotsGen::kActionRects[wide ? 0 : 1],
		CalypsoF07CraftPilotsGen::kActionCount, id);
}

inline CalypsoLogicalRect f07CraftPilotsDetailActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF07CraftPilotsGen::kDetailActionRects[wide ? 0 : 1],
		CalypsoF07CraftPilotsGen::kDetailActionCount, id);
}

inline CalypsoLogicalRect f07CraftPilotsDetailMetricRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF07CraftPilotsGen::kDetailMetricRects[wide ? 0 : 1],
		CalypsoF07CraftPilotsGen::kDetailMetricCount, id);
}

/// Approved Remove-All enablement readout (§7.4): native visibility compares
/// the onboard-qualified diver count against required seats, so it can hide
/// while assigned pilots are still aboard. The HD readout derives from the
/// actual assigned-pilot count instead: zero (none aboard, nothing to clear)
/// hides; partial/full/surplus-qualified assigned counts show. Pure and
/// natively unit-testable; add/remove semantics are untouched. This corrects
/// the control's enablement readout only.
inline bool calypsoF07RemoveAllReadoutVisible(std::size_t assignedPilots)
{
	return assignedPilots > 0;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
