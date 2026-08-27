#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class MissionDetectedState; namespace Calypso { class CalypsoF17MissionDetectedUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF17MissionDetectedUi(MissionDetectedState* s) : _state(s) {}
    ~CalypsoF17MissionDetectedUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(MissionDetectedState& s, bool allow=true);
    static bool resize(MissionDetectedState& s);
private:
    static void applyLayout(MissionDetectedState& s);
    MissionDetectedState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
