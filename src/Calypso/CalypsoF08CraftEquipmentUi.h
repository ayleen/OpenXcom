#pragma once
#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoTabbedManagementRenderer.h"
namespace OpenXcom { class CraftEquipmentState; namespace Calypso {
 // F08 submarine-equipment production adapter (shared tabbed-management
 // renderer). The native item list stays the behavior/input owner:
 // arrow press/hold/click and wheel move stores on and off the craft
 // immediately (with hold acceleration and the bulk right-click paths),
 // middle-click opens the item article, the category filter keeps its
 // native handler on its generated control slot, the quick search editor
 // sits on its generated footer slot, and Clear/Inventory keep their
 // native buttons, visibility gates, and keyboard paths on their own
 // generated slots (X and keyBattleInventory survive). Load/Save keep
 // their keyboard owners (F5/F9) without painted pointer hit areas:
 // both resolve to no native widget, so painting them would be a
 // dead-end. Research gates, mission restrictions, ground/craft
 // sourcing, bulk-transfer behavior, vehicle deployment invalidation,
 // the exact transfer ErrorMessageState handoffs, and the non-atomic
 // preset result (CannotReequipState reach-and-return only) are
 // untouched native semantics. Store plus on-board quantities paint
 // verbatim from the live native cells every frame.
 class CalypsoF08CraftEquipmentUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF08CraftEquipmentUi(CraftEquipmentState* s) : _state(s) {}
    ~CalypsoF08CraftEquipmentUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: equipment chrome must stay hidden under pushed
    // children (preset pickers, Inventory, ErrorMessage, CannotReequip)
    // instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftEquipmentState& s);
    static bool resize(CraftEquipmentState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct ItemRows
    {
        std::vector<std::string> ids;
        std::vector<CalypsoTabbedRow> rows;
    };
    static ItemRows itemRows(const CraftEquipmentState& state);
    static void applyGeneratedLayout(CraftEquipmentState& s, bool wide);
    CraftEquipmentState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
