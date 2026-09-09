#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldiersState; namespace Calypso { class CalypsoF04SoldiersUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF04SoldiersUi(SoldiersState* s) : _state(s) {}
    ~CalypsoF04SoldiersUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: roster chrome must stay hidden under pushed children
    // (profile, memorial, allocators, inventory) instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldiersState& s);
    static bool resize(SoldiersState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct SoldierRows;
    static SoldierRows soldierRows(const SoldiersState& state);
    static void applyGeneratedLayout(SoldiersState& s, bool wide);
    SoldiersState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
