#pragma once
#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoTabbedManagementRenderer.h"
namespace OpenXcom { class CraftArmorState; namespace Calypso {
 // F08 submarine-armor production adapter (shared tabbed-management
 // renderer). The native per-diver list stays the behavior/input owner:
 // left-click opens the existing per-diver armor route
 // (SoldierArmorState, SA_GEOSCAPE), Ctrl+click runs the immediate
 // assign/remove exchange, right-click applies the quick armor, and
 // middle-click opens the armor article — all through the unchanged
 // native handlers, which also own research/compatibility/stock/
 // capacity gates, the immediate inventory exchange, the default-armor
 // resets, and the group/capacity ErrorMessageState handoffs. The sort
 // combobox keeps its native handler on its generated control slot; the
 // keyboard-only de-equip actions keep their native owners without
 // painted pointer hit areas. Keyboard paths and native gates behave
 // exactly as before.
 class CalypsoF08CraftArmorUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF08CraftArmorUi(CraftArmorState* s) : _state(s) {}
    ~CalypsoF08CraftArmorUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: armor chrome must stay hidden under pushed children
    // (per-diver armor route, ErrorMessage) instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftArmorState& s);
    static bool resize(CraftArmorState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct SoldierRows
    {
        std::vector<std::string> ids;
        std::vector<CalypsoTabbedRow> rows;
    };
    static SoldierRows soldierRows(const CraftArmorState& state);
    static void applyGeneratedLayout(CraftArmorState& s, bool wide);
    CraftArmorState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
