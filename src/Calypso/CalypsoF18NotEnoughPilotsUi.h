#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class CraftNotEnoughPilotsState; namespace Calypso { class CalypsoF18NotEnoughPilotsUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF18NotEnoughPilotsUi(CraftNotEnoughPilotsState* s) : _state(s) {}
    ~CalypsoF18NotEnoughPilotsUi() override;
    const void* topState() const override;
    const void* physicalUnderlayState() const override { return _underlay; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftNotEnoughPilotsState& s, bool allow=true);
    static bool resize(CraftNotEnoughPilotsState& s);
private:
    CraftNotEnoughPilotsState* _state=nullptr;
    const void* _underlay=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
