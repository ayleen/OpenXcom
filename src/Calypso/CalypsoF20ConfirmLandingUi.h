#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class ConfirmLandingState; namespace Calypso { class CalypsoF20ConfirmLandingUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF20ConfirmLandingUi(ConfirmLandingState* s) : _state(s) {}
    ~CalypsoF20ConfirmLandingUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(ConfirmLandingState& s, bool allow=true);
    static bool resize(ConfirmLandingState& s);
private:
    ConfirmLandingState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
