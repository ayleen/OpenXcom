#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierDiaryLightState; namespace Calypso {
// F06 combat-totals production adapter: the Battlescape/inventory weapon
// popup as the shared-shell Combat context (C4). Read-only; the native list
// stays the scroll owner, OK pops back to the entry context. Exact native
// neutralization breakdowns, no invented totals.
class CalypsoF06DiaryLightUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF06DiaryLightUi(SoldierDiaryLightState* s) : _state(s) {}
    ~CalypsoF06DiaryLightUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierDiaryLightState& s);
    static bool resize(SoldierDiaryLightState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct LightRows;
    static LightRows lightRows(const SoldierDiaryLightState& state);
    static void applyGeneratedLayout(SoldierDiaryLightState& s, bool wide);
    SoldierDiaryLightState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
