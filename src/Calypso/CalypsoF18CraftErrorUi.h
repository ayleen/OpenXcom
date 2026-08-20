#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class CraftErrorState; namespace Calypso { class CalypsoF18CraftErrorUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF18CraftErrorUi(CraftErrorState* s) : _state(s) {}
    ~CalypsoF18CraftErrorUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftErrorState& s, bool allow=true);
    static bool resize(CraftErrorState& s);
private:
    CraftErrorState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
