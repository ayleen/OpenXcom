#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierRankState; namespace Calypso { class CalypsoF04SoldierRankUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF04SoldierRankUi(SoldierRankState* s) : _state(s) {}
    ~CalypsoF04SoldierRankUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: rank chrome must stay hidden under pushed children
    // (rank Ufopaedia articles) instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierRankState& s);
    static bool resize(SoldierRankState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct RankRows;
    static RankRows rankRows(const SoldierRankState& state);
    static void applyGeneratedLayout(SoldierRankState& s, bool wide);
    SoldierRankState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
