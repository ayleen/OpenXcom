#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class TransferConfirmState; namespace Calypso { class CalypsoF12TransferConfirmUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF12TransferConfirmUi(TransferConfirmState* s) : _state(s) {}
    ~CalypsoF12TransferConfirmUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(TransferConfirmState& s, bool allow=true);
    static bool resize(TransferConfirmState& s);
private:
    TransferConfirmState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
