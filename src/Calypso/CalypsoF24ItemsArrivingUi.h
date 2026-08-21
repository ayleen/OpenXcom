#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class ItemsArrivingState; namespace Calypso { class CalypsoF24ItemsArrivingUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF24ItemsArrivingUi(ItemsArrivingState* s) : _state(s) {}
    ~CalypsoF24ItemsArrivingUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(ItemsArrivingState& s, bool allow=true);
    static bool resize(ItemsArrivingState& s);
private:
    ItemsArrivingState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
