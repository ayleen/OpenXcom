#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class ConfirmCydoniaState; namespace Calypso { class CalypsoF20ConfirmCydoniaUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF20ConfirmCydoniaUi(ConfirmCydoniaState* s) : _state(s) {}
    ~CalypsoF20ConfirmCydoniaUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(ConfirmCydoniaState& s, bool allow=true);
    static bool resize(ConfirmCydoniaState& s);
private:
    ConfirmCydoniaState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
