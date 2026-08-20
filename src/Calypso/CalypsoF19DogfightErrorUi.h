#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class DogfightErrorState; namespace Calypso { class CalypsoF19DogfightErrorUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF19DogfightErrorUi(DogfightErrorState* s) : _state(s) {}
    ~CalypsoF19DogfightErrorUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(DogfightErrorState& s, bool allow=true);
    static bool resize(DogfightErrorState& s);
private:
    DogfightErrorState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
