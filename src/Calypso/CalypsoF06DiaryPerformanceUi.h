#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierDiaryPerformanceState; namespace Calypso {
// F06 performance production adapter: Kills/Missions/Commendations grouped
// views of the same engine diary data (shared selection-list renderer, C4).
// The visible native list stays the behavior/input owner; commendation
// sprites stay live native surfaces repositioned onto the painted row slots
// (content follows scroll through the native drawSprites path). Mind-control
// rows appear only under the existing psi conditions; NO_UFO records stay
// excluded; commendation ordering stays localized.
class CalypsoF06DiaryPerformanceUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF06DiaryPerformanceUi(SoldierDiaryPerformanceState* s) : _state(s) {}
    ~CalypsoF06DiaryPerformanceUi() override;
    const void* topState() const override;
    // The live commendation sprites stay visible native data (ruleset raster,
    // not chrome): whole-state suppression is off and every chrome widget is
    // listed explicitly instead (avatar precedent).
    bool suppressLogicalState() const override { return false; }
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierDiaryPerformanceState& s);
    static bool resize(SoldierDiaryPerformanceState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct PerformanceRows;
    static PerformanceRows performanceRows(const SoldierDiaryPerformanceState& state);
    static void applyGeneratedLayout(SoldierDiaryPerformanceState& s, bool wide);
    SoldierDiaryPerformanceState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
