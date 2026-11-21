#pragma once
#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoTabbedManagementRenderer.h"
namespace OpenXcom { class CraftEquipmentLoadState; class Game; namespace Calypso {
 // F08 preset-load production adapter (shared tabbed-management
 // renderer). The native slot list stays the behavior/input owner: row
 // tap applies the existing preset path (filter reset, full-list reload,
 // Replace vs Add-on-top from the live toggle or held Ctrl, partial
 // application with the CannotReequipState shortage disclosure) and
 // Cancel pops non-mutating. Critical safety (§8.3): empty slots paint
 // disabled AND the native apply handler rejects them before the pop and
 // before any search/filter reset or inventory mutation — a native empty
 // Replace would otherwise clear the craft without warning. Rejection is
 // non-mutating and silent (disabled-control semantics); the paint and
 // the guard share one predicate. The Add-on-top toggle keeps its native
 // owner, handler, pressed state, and Ctrl path on its generated control
 // slot, so modes are never shortcut-only and no parked toggle hit area
 // survives.
 class CalypsoF08CraftEquipmentLoadUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF08CraftEquipmentLoadUi(CraftEquipmentLoadState* s) : _state(s) {}
    ~CalypsoF08CraftEquipmentLoadUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: picker chrome must stay hidden under pushed
    // children (CannotReequip, ErrorMessage) instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftEquipmentLoadState& s);
    static bool resize(CraftEquipmentLoadState& s);
    // Empty-preset predicate shared by the disabled paint and the native
    // apply guard: unknown or out-of-range slots count as empty (fail
    // closed — an unknown slot never applies).
    static bool isEmptyPreset(Game* game, int row);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct SlotRows
    {
        std::vector<std::string> ids;
        std::vector<CalypsoTabbedRow> rows;
    };
    static SlotRows slotRows(const CraftEquipmentLoadState& state);
    static void applyGeneratedLayout(CraftEquipmentLoadState& s, bool wide);
    CraftEquipmentLoadState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
