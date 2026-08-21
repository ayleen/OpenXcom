#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class AbortMissionState; namespace Calypso { class CalypsoF28AbortMissionUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF28AbortMissionUi(AbortMissionState* s) : _state(s) {}
    ~CalypsoF28AbortMissionUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(AbortMissionState& s, bool allow=true);
    static bool resize(AbortMissionState& s);
private:
    AbortMissionState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
