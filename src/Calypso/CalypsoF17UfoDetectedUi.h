#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class UfoDetectedState; namespace Calypso { class CalypsoF17UfoDetectedUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF17UfoDetectedUi(UfoDetectedState* s) : _state(s) {}
    ~CalypsoF17UfoDetectedUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(UfoDetectedState& s, bool allow=true);
    static bool resize(UfoDetectedState& s);
private:
    UfoDetectedState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
