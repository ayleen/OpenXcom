#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class ResearchCompleteState; namespace Calypso { class CalypsoF24ResearchCompleteUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF24ResearchCompleteUi(ResearchCompleteState* s) : _state(s) {}
    ~CalypsoF24ResearchCompleteUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(ResearchCompleteState& s, bool allow=true);
    static bool resize(ResearchCompleteState& s);
private:
    ResearchCompleteState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
