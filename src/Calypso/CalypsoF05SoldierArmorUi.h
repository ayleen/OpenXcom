#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierArmorState; namespace Calypso { class CalypsoF05SoldierArmorUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF05SoldierArmorUi(SoldierArmorState* s) : _state(s) {}
    ~CalypsoF05SoldierArmorUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierArmorState& s);
    static bool resize(SoldierArmorState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct ArmorRows;
    static ArmorRows armorRows(const SoldierArmorState& state);
    static void applyGeneratedLayout(SoldierArmorState& s, bool wide);
    SoldierArmorState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
