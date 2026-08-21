#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SackSoldierState; namespace Calypso { class CalypsoF04SackSoldierUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF04SackSoldierUi(SackSoldierState* s) : _state(s) {}
    ~CalypsoF04SackSoldierUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SackSoldierState& s, bool allow=true);
    static bool resize(SackSoldierState& s);
private:
    SackSoldierState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
