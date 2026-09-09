#pragma once
#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoTabbedManagementRenderer.h"
namespace OpenXcom { class CraftEquipmentSaveState; namespace Calypso {
 // F08 preset-save production adapter (shared tabbed-management
 // renderer). The native slot list stays the behavior/input owner: row
 // press selects and inlines the live name editor (Return commits,
 // right-click cancels), and the existing save path (slot name plus
 // saveGlobalLoadout over the visible item scope, then pop) is
 // unchanged. Overwrite rule (§8.3, C1): saving to an empty slot commits
 // directly through the unchanged path, while a non-empty slot requires
 // the explicit two-press review — the first Save arms the commit and
 // relabels the button, the second Save on the same slot invokes the
 // unchanged save mutation. Arming is slot-bound, so reselecting always
 // re-arms; the Return key travels the same gate. The inline editor keeps
 // its native position, focus, commit, and cancel behavior; its live text
 // mirrors into the generated name control (verbatim native strings,
 // zero invented copy). Save and Cancel both bind their generated footer
 // slots with live labels, so the commit it guards stays
 // pointer-reachable and no parked footer hit area survives.
 class CalypsoF08CraftEquipmentSaveUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF08CraftEquipmentSaveUi(CraftEquipmentSaveState* s) : _state(s) {}
    ~CalypsoF08CraftEquipmentSaveUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: picker chrome must stay hidden under pushed
    // children (ErrorMessage) instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftEquipmentSaveState& s);
    static bool resize(CraftEquipmentSaveState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct SlotRows
    {
        std::vector<std::string> ids;
        std::vector<CalypsoTabbedRow> rows;
    };
    static SlotRows slotRows(const CraftEquipmentSaveState& state);
    static void applyGeneratedLayout(CraftEquipmentSaveState& s, bool wide);
    CraftEquipmentSaveState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
