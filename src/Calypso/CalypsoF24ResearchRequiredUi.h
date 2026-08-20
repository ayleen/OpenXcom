#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class ResearchRequiredState; namespace Calypso { class CalypsoF24ResearchRequiredUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF24ResearchRequiredUi(ResearchRequiredState* s) : _state(s) {}
    ~CalypsoF24ResearchRequiredUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(ResearchRequiredState& s, bool allow=true);
    static bool resize(ResearchRequiredState& s);
private:
    ResearchRequiredState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
