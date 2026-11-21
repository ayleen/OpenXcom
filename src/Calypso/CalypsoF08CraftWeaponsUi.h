#pragma once
#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoTabbedManagementRenderer.h"
namespace OpenXcom { class CraftWeaponsState; namespace Calypso {
 // F08 submarine-weapons production adapter (shared tabbed-management
 // renderer). The native mount list stays the behavior/input owner: row
 // click runs the immediate launcher/clip exchange (the Change route:
 // capacity pre-checks, old launcher plus loaded clips back to storage,
 // new launcher consumed, stats adjusted, shield clamped, checkup, pop)
 // and middle-click opens the reference article (the More route) — all
 // through the unchanged native handlers, which also own candidate
 // eligibility (researched launcher and clip requirements, launcher
 // stock, valid ruleset slot) and the exact cargo/HWP/storage
 // ErrorMessageState handoffs (picker popped first, localized text
 // preserved). The contract's Change/More detail entries resolve to no
 // native widget, so they stay unpainted instead of dead-end controls;
 // both routes stay reachable through the list. Rearming stays automatic
 // disclosure from the mount status: no manual reload exists and none
 // is added. The contextual reference shortcut keeps working for
 // mouse users; touch-initiated reference needs a native touch widget
 // that does not exist yet (reported shell follow-up, no new bindings).
 class CalypsoF08CraftWeaponsUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF08CraftWeaponsUi(CraftWeaponsState* s) : _state(s) {}
    ~CalypsoF08CraftWeaponsUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: weapons chrome must stay hidden under pushed
    // children (reference article, ErrorMessage) instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftWeaponsState& s);
    static bool resize(CraftWeaponsState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct WeaponRows
    {
        std::vector<std::string> ids;
        std::vector<CalypsoTabbedRow> rows;
    };
    static WeaponRows weaponRows(const CraftWeaponsState& state);
    static void applyGeneratedLayout(CraftWeaponsState& s, bool wide);
    CraftWeaponsState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
