#pragma once
#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierTransformationState; namespace Calypso { class CalypsoF05SoldierTransformationUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF05SoldierTransformationUi(SoldierTransformationState* s) : _state(s) {}
    ~CalypsoF05SoldierTransformationUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierTransformationState& s);
    static bool resize(SoldierTransformationState& s);
    // Rebuilds the adapter-owned inspector list from live native widgets and
    // rule reads (cost/funds-after, times, required items, quarters result,
    // consequences, stat changes). Called outside collection from configure
    // and from the native initTransformationData path (personnel cycling).
    static void refreshReviewRows(SoldierTransformationState& state);
private:
    static void applyGeneratedLayout(SoldierTransformationState& s, bool wide);
    SoldierTransformationState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
    // Per-row availability verdicts shadowing the inspector list rows
    // (shortfall rows paint muted, never silent).
    std::vector<char> _rowEnabled;
}; } }
#endif
