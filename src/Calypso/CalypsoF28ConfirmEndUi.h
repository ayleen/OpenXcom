#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class ConfirmEndMissionState; namespace Calypso { class CalypsoF28ConfirmEndUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF28ConfirmEndUi(ConfirmEndMissionState* s) : _state(s) {}
    ~CalypsoF28ConfirmEndUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(ConfirmEndMissionState& s, bool allow=true);
    static bool resize(ConfirmEndMissionState& s);
private:
    ConfirmEndMissionState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
