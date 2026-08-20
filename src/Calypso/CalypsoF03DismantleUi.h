#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class DismantleFacilityState; namespace Calypso { class CalypsoF03DismantleUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF03DismantleUi(DismantleFacilityState* s) : _state(s) {}
    ~CalypsoF03DismantleUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(DismantleFacilityState& s, bool allow=true);
    static bool resize(DismantleFacilityState& s);
private:
    DismantleFacilityState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
