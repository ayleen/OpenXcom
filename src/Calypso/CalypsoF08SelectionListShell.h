#pragma once
// F08 submarine crew/loadout/armament shared tabbed-management shell.
//
// Binds the six F08 generated tabbed contracts (full-viewport submarine
// compositions per the stage-1 boards, §6.1). Layout lookup is fail-closed;
// geometry plumbing (projection, named-rect search, list seam, touch floors)
// is archetype-shared in CalypsoTabbedManagementRenderer.h. Rendering stays
// with the shared tabbed collector, never here. All six F08 routes (crew,
// armor, weapons, equipment, preset load, preset save) are covered; the
// family gate lands separately.
//
// Parked controls stay fully live input/behavior owners: only their blit is
// suppressed and only their hit area moves (visibility flags and handler
// bindings are never modified), so keyboard paths and native gates behave
// exactly as before while no vanilla pixel can show or intercept pointer.
#ifdef __EMSCRIPTEN__
#include <string>
#include "../Engine/Surface.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoTabbedManagementRenderer.h"
#include "Generated/CalypsoF08CraftSoldiers.generated.h"
#include "Generated/CalypsoF08CraftArmor.generated.h"
#include "Generated/CalypsoF08CraftWeapons.generated.h"
#include "Generated/CalypsoF08CraftEquipment.generated.h"
#include "Generated/CalypsoF08CraftEquipmentLoad.generated.h"
#include "Generated/CalypsoF08CraftEquipmentSave.generated.h"

namespace OpenXcom
{
namespace Calypso
{

inline bool f08HdWideLayout()
{
	return calypsoTabbedWideLayout();
}

inline const CalypsoF08CraftSoldiersGen::CalypsoF08CraftSoldiersGenLayout* f08CraftSoldiersLayout(bool wide)
{
	const auto* generated = CalypsoF08CraftSoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 crew generated layout is missing");
	return generated;
}

inline CalypsoLogicalRect f08CraftSoldiersTabRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftSoldiersGen::kTabRects[wide ? 0 : 1],
		CalypsoF08CraftSoldiersGen::kTabCount, id);
}

inline CalypsoLogicalRect f08CraftSoldiersActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftSoldiersGen::kActionRects[wide ? 0 : 1],
		CalypsoF08CraftSoldiersGen::kActionCount, id);
}

inline CalypsoLogicalRect f08CraftSoldiersControlRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftSoldiersGen::kControlRects[wide ? 0 : 1],
		CalypsoF08CraftSoldiersGen::kControlCount, id);
}

inline CalypsoLogicalRect f08CraftSoldiersDetailActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftSoldiersGen::kDetailActionRects[wide ? 0 : 1],
		CalypsoF08CraftSoldiersGen::kDetailActionCount, id);
}

inline CalypsoLogicalRect f08CraftSoldiersDetailMetricRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftSoldiersGen::kDetailMetricRects[wide ? 0 : 1],
		CalypsoF08CraftSoldiersGen::kDetailMetricCount, id);
}

inline const CalypsoF08CraftArmorGen::CalypsoF08CraftArmorGenLayout* f08CraftArmorLayout(bool wide)
{
	const auto* generated = CalypsoF08CraftArmorGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 armor generated layout is missing");
	return generated;
}

inline CalypsoLogicalRect f08CraftArmorTabRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftArmorGen::kTabRects[wide ? 0 : 1],
		CalypsoF08CraftArmorGen::kTabCount, id);
}

inline CalypsoLogicalRect f08CraftArmorActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftArmorGen::kActionRects[wide ? 0 : 1],
		CalypsoF08CraftArmorGen::kActionCount, id);
}

inline CalypsoLogicalRect f08CraftArmorControlRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftArmorGen::kControlRects[wide ? 0 : 1],
		CalypsoF08CraftArmorGen::kControlCount, id);
}

inline const CalypsoF08CraftWeaponsGen::CalypsoF08CraftWeaponsGenLayout* f08CraftWeaponsLayout(bool wide)
{
	const auto* generated = CalypsoF08CraftWeaponsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 weapons generated layout is missing");
	return generated;
}

inline CalypsoLogicalRect f08CraftWeaponsTabRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftWeaponsGen::kTabRects[wide ? 0 : 1],
		CalypsoF08CraftWeaponsGen::kTabCount, id);
}

inline CalypsoLogicalRect f08CraftWeaponsActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftWeaponsGen::kActionRects[wide ? 0 : 1],
		CalypsoF08CraftWeaponsGen::kActionCount, id);
}

inline CalypsoLogicalRect f08CraftWeaponsDetailActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftWeaponsGen::kDetailActionRects[wide ? 0 : 1],
		CalypsoF08CraftWeaponsGen::kDetailActionCount, id);
}

inline CalypsoLogicalRect f08CraftWeaponsDetailMetricRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftWeaponsGen::kDetailMetricRects[wide ? 0 : 1],
		CalypsoF08CraftWeaponsGen::kDetailMetricCount, id);
}

inline const CalypsoF08CraftEquipmentGen::CalypsoF08CraftEquipmentGenLayout* f08CraftEquipmentLayout(bool wide)
{
	const auto* generated = CalypsoF08CraftEquipmentGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 equipment generated layout is missing");
	return generated;
}

inline CalypsoLogicalRect f08CraftEquipmentTabRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftEquipmentGen::kTabRects[wide ? 0 : 1],
		CalypsoF08CraftEquipmentGen::kTabCount, id);
}

inline CalypsoLogicalRect f08CraftEquipmentActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftEquipmentGen::kActionRects[wide ? 0 : 1],
		CalypsoF08CraftEquipmentGen::kActionCount, id);
}

inline CalypsoLogicalRect f08CraftEquipmentToolbarRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftEquipmentGen::kToolbarRects[wide ? 0 : 1],
		CalypsoF08CraftEquipmentGen::kToolbarCount, id);
}

inline CalypsoLogicalRect f08CraftEquipmentControlRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftEquipmentGen::kControlRects[wide ? 0 : 1],
		CalypsoF08CraftEquipmentGen::kControlCount, id);
}

inline const CalypsoF08CraftEquipmentLoadGen::CalypsoF08CraftEquipmentLoadGenLayout* f08CraftEquipmentLoadLayout(bool wide)
{
	const auto* generated = CalypsoF08CraftEquipmentLoadGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 preset-load generated layout is missing");
	return generated;
}

inline CalypsoLogicalRect f08CraftEquipmentLoadTabRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftEquipmentLoadGen::kTabRects[wide ? 0 : 1],
		CalypsoF08CraftEquipmentLoadGen::kTabCount, id);
}

inline CalypsoLogicalRect f08CraftEquipmentLoadActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftEquipmentLoadGen::kActionRects[wide ? 0 : 1],
		CalypsoF08CraftEquipmentLoadGen::kActionCount, id);
}

inline CalypsoLogicalRect f08CraftEquipmentLoadControlRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftEquipmentLoadGen::kControlRects[wide ? 0 : 1],
		CalypsoF08CraftEquipmentLoadGen::kControlCount, id);
}

inline const CalypsoF08CraftEquipmentSaveGen::CalypsoF08CraftEquipmentSaveGenLayout* f08CraftEquipmentSaveLayout(bool wide)
{
	const auto* generated = CalypsoF08CraftEquipmentSaveGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 preset-save generated layout is missing");
	return generated;
}

inline CalypsoLogicalRect f08CraftEquipmentSaveTabRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftEquipmentSaveGen::kTabRects[wide ? 0 : 1],
		CalypsoF08CraftEquipmentSaveGen::kTabCount, id);
}

inline CalypsoLogicalRect f08CraftEquipmentSaveActionRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftEquipmentSaveGen::kActionRects[wide ? 0 : 1],
		CalypsoF08CraftEquipmentSaveGen::kActionCount, id);
}

inline CalypsoLogicalRect f08CraftEquipmentSaveControlRect(bool wide, const char* id)
{
	return calypsoTabbedFindRect(CalypsoF08CraftEquipmentSaveGen::kControlRects[wide ? 0 : 1],
		CalypsoF08CraftEquipmentSaveGen::kControlCount, id);
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
