#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class ProductionCompleteState; namespace Calypso { class CalypsoF24ProductionCompleteUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF24ProductionCompleteUi(ProductionCompleteState* s) : _state(s) {}
    ~CalypsoF24ProductionCompleteUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(ProductionCompleteState& s, bool allow=true);
    static bool resize(ProductionCompleteState& s);
private:
    ProductionCompleteState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
