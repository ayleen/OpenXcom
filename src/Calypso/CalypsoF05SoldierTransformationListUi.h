#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierTransformationListState; namespace Calypso { class CalypsoF05SoldierTransformationListUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF05SoldierTransformationListUi(SoldierTransformationListState* s) : _state(s) {}
    ~CalypsoF05SoldierTransformationListUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierTransformationListState& s);
    static bool resize(SoldierTransformationListState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct OverviewRows;
    static OverviewRows overviewRows(const SoldierTransformationListState& state);
    static void applyGeneratedLayout(SoldierTransformationListState& s, bool wide);
    SoldierTransformationListState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
