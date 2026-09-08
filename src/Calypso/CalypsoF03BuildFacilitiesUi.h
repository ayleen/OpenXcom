#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class BuildFacilitiesState; namespace Calypso { class CalypsoF03BuildFacilitiesUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF03BuildFacilitiesUi(BuildFacilitiesState* s) : _state(s) {}
    ~CalypsoF03BuildFacilitiesUi() override;
    const void* topState() const override;
    const void* physicalUnderlayState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: the picker's native window/list/footer must stay
    // hidden under a child outside the composed chain.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(BuildFacilitiesState& s);
    static bool resize(BuildFacilitiesState& s);
private:
    static void applyGeneratedLayout(BuildFacilitiesState& s, bool wide);
    BuildFacilitiesState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
