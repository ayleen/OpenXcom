#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class TrainingFinishedState; namespace Calypso { class CalypsoF22TrainingFinishedUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF22TrainingFinishedUi(TrainingFinishedState* s) : _state(s) {}
    ~CalypsoF22TrainingFinishedUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(TrainingFinishedState& s, bool allow=true);
    static bool resize(TrainingFinishedState& s);
private:
    TrainingFinishedState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
