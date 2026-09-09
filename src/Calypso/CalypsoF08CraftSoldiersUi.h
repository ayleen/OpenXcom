#pragma once
#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoTabbedManagementRenderer.h"
namespace OpenXcom { class CraftSoldiersState; namespace Calypso {
 // F08 submarine-crew production adapter (shared tabbed-management
 // renderer). The native diver list stays the behavior/input owner:
 // left-click toggles assignment immediately, right-click opens the
 // existing per-diver profile route, arrows/wheel reorder deployment,
 // the sort combobox keeps its native handler on its generated control
 // slot, and Done keeps the pop behavior. Error handoffs (soldier/armor
 // group, craft space) and the Deployment preview handoff
 // (BriefingState(craft)) run through the unchanged native handlers.
 // Capacity scope (space available/used) paints verbatim from the live
 // native texts; full capacity and STR_OUT-craft rows paint disabled as
 // a readout only (the native click no-ops there today). The Deployment
 // preview button binds its generated detail slot exactly while its
 // native visibility gate allows; the keyboard-only de-assign actions
 // keep their native owners without painted pointer hit areas.
 // Keyboard paths and native gates behave exactly as before.
 class CalypsoF08CraftSoldiersUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF08CraftSoldiersUi(CraftSoldiersState* s) : _state(s) {}
    ~CalypsoF08CraftSoldiersUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: crew chrome must stay hidden under pushed children
    // (profile, Deployment preview, ErrorMessage) instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftSoldiersState& s);
    static bool resize(CraftSoldiersState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct SoldierRows
    {
        std::vector<std::string> ids;
        std::vector<CalypsoTabbedRow> rows;
    };
    static SoldierRows soldierRows(const CraftSoldiersState& state);
    static void applyGeneratedLayout(CraftSoldiersState& s, bool wide);
    CraftSoldiersState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
