#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierMemorialState; namespace Calypso {
// F06 memorial production adapter: campaign-wide dead-soldier ledger over the
// native newest-first list (shared selection-list renderer). The native list
// stays the behavior/input owner (row click into the dead profile, quick
// search, scroll); the Geoscape music transition and Statistics handoff keep
// their existing handlers. Memorial is never embedded in diary screens.
class CalypsoF06SoldierMemorialUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF06SoldierMemorialUi(SoldierMemorialState* s) : _state(s) {}
    ~CalypsoF06SoldierMemorialUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: memorial chrome must stay hidden under pushed
    // children (dead profile, diary, statistics) instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierMemorialState& s);
    static bool resize(SoldierMemorialState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct MemorialRows;
    static MemorialRows memorialRows(const SoldierMemorialState& state);
    static void applyGeneratedLayout(SoldierMemorialState& s, bool wide);
    SoldierMemorialState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
