#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class ManufactureInfoState; namespace Calypso { class CalypsoF10ManufactureCheckUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF10ManufactureCheckUi(ManufactureInfoState* s) : _state(s) {}
    ~CalypsoF10ManufactureCheckUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(ManufactureInfoState& s, bool allow=true);
    static bool resize(ManufactureInfoState& s);
private:
    ManufactureInfoState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
