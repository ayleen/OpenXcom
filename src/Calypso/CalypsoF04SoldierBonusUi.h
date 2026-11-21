#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierBonusState; class TextList; namespace Calypso { class CalypsoF04SoldierBonusUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF04SoldierBonusUi(SoldierBonusState* s) : _state(s) {}
    ~CalypsoF04SoldierBonusUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: bonus chrome must stay hidden under pushed children
    // (Stats-for-Nerds articles) instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierBonusState& s);
    static bool resize(SoldierBonusState& s);
private:
    // Row snapshot and active-view resolution over live native list state.
    // Private static members (not namespace-scope helpers) so state
    // friendship covers the private widget reads; friendship is not
    // transitive to free functions.
    struct BonusRows;
    static BonusRows bonusRows(const SoldierBonusState& state);
    static TextList* bonusActiveList(SoldierBonusState& state);
    static void applyGeneratedLayout(SoldierBonusState& s, bool wide);
    SoldierBonusState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
