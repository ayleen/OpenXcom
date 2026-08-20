#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierDiaryOverviewState; namespace Calypso { class CalypsoF06SoldierDiaryUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF06SoldierDiaryUi(SoldierDiaryOverviewState* s) : _state(s) {}
    ~CalypsoF06SoldierDiaryUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierDiaryOverviewState& s, bool allow=true);
    static bool resize(SoldierDiaryOverviewState& s);
private:
    SoldierDiaryOverviewState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
