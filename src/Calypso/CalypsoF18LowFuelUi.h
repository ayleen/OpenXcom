#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class LowFuelState; namespace Calypso { class CalypsoF18LowFuelUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF18LowFuelUi(LowFuelState* s) : _state(s) {}
    ~CalypsoF18LowFuelUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(LowFuelState& s, bool allow=true);
    static bool resize(LowFuelState& s);
private:
    LowFuelState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
