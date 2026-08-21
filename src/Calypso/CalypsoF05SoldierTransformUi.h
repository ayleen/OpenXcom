#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierTransformState; namespace Calypso { class CalypsoF05SoldierTransformUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF05SoldierTransformUi(SoldierTransformState* s) : _state(s) {}
    ~CalypsoF05SoldierTransformUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierTransformState& s, bool allow=true);
    static bool resize(SoldierTransformState& s);
private:
    SoldierTransformState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
