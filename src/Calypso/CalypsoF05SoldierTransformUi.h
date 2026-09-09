#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierTransformState; namespace Calypso { class CalypsoF05SoldierTransformUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF05SoldierTransformUi(SoldierTransformState* s) : _state(s) {}
    ~CalypsoF05SoldierTransformUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierTransformState& s, bool allow=true);
    static bool resize(SoldierTransformState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct TransformRows;
    static TransformRows transformRows(const SoldierTransformState& state);
    static void applyGeneratedLayout(SoldierTransformState& s, bool wide);
    SoldierTransformState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
