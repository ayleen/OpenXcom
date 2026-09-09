#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierDiaryOverviewState; namespace Calypso {
// F06 diary-overview production adapter: Personnel History shell over the
// native mission ledger (shared selection-list renderer, C4). The native
// mission list stays the behavior/input owner (row click, scroll); parked
// tab/personnel controls stay live for handlers, gates, and keyboard paths.
class CalypsoF06SoldierDiaryUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF06SoldierDiaryUi(SoldierDiaryOverviewState* s) : _state(s) {}
    ~CalypsoF06SoldierDiaryUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: diary chrome must stay hidden under pushed children
    // (mission detail, performance, profile, Ufopaedia) instead of leaking.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierDiaryOverviewState& s);
    static bool resize(SoldierDiaryOverviewState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct DiaryRows;
    static DiaryRows diaryRows(const SoldierDiaryOverviewState& state);
    static void applyGeneratedLayout(SoldierDiaryOverviewState& s, bool wide);
    SoldierDiaryOverviewState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
