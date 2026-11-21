#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierDiaryMissionState; namespace Calypso {
// F06 mission-detail production adapter: read-only per-mission record over
// the native conditional fields and kill list (shared selection-list
// renderer). Every conditional field is reinitialized per mission by the
// native init(); unknown fields stay absent, never misleading. Unit-vs-race
// distinction and the explicit No-record branch are preserved verbatim.
class CalypsoF06DiaryMissionUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF06DiaryMissionUi(SoldierDiaryMissionState* s) : _state(s) {}
    ~CalypsoF06DiaryMissionUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierDiaryMissionState& s);
    static bool resize(SoldierDiaryMissionState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct MissionRows;
    static MissionRows missionRows(const SoldierDiaryMissionState& state);
    static void applyGeneratedLayout(SoldierDiaryMissionState& s, bool wide);
    SoldierDiaryMissionState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
