#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class NoExperienceState; namespace Calypso { class CalypsoF30NoExperienceUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF30NoExperienceUi(NoExperienceState* s) : _state(s) {}
    ~CalypsoF30NoExperienceUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(NoExperienceState& s, bool allow=true);
    static bool resize(NoExperienceState& s);
private:
    NoExperienceState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
