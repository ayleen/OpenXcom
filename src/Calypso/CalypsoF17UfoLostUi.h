#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class UfoLostState; namespace Calypso { class CalypsoF17UfoLostUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF17UfoLostUi(UfoLostState* s) : _state(s) {}
    ~CalypsoF17UfoLostUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(UfoLostState& s, bool allow=true);
    static bool resize(UfoLostState& s);
private:
    UfoLostState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
