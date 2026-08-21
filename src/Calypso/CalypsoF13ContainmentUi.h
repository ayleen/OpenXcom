#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class ManageAlienContainmentState; namespace Calypso { class CalypsoF13ContainmentUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF13ContainmentUi(ManageAlienContainmentState* s) : _state(s) {}
    ~CalypsoF13ContainmentUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(ManageAlienContainmentState& s, bool allow=true);
    static bool resize(ManageAlienContainmentState& s);
private:
    ManageAlienContainmentState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
