#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierAvatarState; namespace Calypso { class CalypsoF05SoldierAvatarUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF05SoldierAvatarUi(SoldierAvatarState* s) : _state(s) {}
    ~CalypsoF05SoldierAvatarUi() override;
    const void* topState() const override;
    // The live avatar preview surface stays visible native data (soldier
    // pixels, not chrome): whole-state suppression is off and every chrome
    // widget is listed explicitly instead.
    bool suppressLogicalState() const override { return false; }
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierAvatarState& s);
    static bool resize(SoldierAvatarState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct AvatarRows;
    static AvatarRows avatarRows(const SoldierAvatarState& state);
    static void applyGeneratedLayout(SoldierAvatarState& s, bool wide);
    SoldierAvatarState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
