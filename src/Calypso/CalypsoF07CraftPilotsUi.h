#pragma once
#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoTabbedManagementRenderer.h"
namespace OpenXcom { class CraftPilotsState; class TextList; namespace Calypso {
 // F07 assigned-pilots production adapter (shared tabbed-management
 // renderer). Paints the pilots tab, the required/assigned summary, the
 // assigned-pilot rows (name + firing/reactions/bravery from
 // getStatsWithSoldierBonusesOnly()), the bonus detail, and the named
 // Add/Clear actions from live native state every frame. Add/Clear resolve
 // to the existing native buttons with immediate add/remove and bonus
 // recalculation on return; each paints exactly while its native widget is
 // visible, so no parked action hit area survives. The Remove-All readout
 // correction (§7.4 quirk) is applied in refreshForInit, separated from
 // layout for its own fix commit.
 class CalypsoF07CraftPilotsUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF07CraftPilotsUi(CraftPilotsState* s) : _state(s) {}
    ~CalypsoF07CraftPilotsUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: pilots chrome must stay hidden under the pushed
    // candidate selector instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftPilotsState& s);
    static bool resize(CraftPilotsState& s);
    // Applies the Remove-All readout correction and re-seats the Add/Clear
    // hit areas. Called from the state's init() tail (native updateUI
    // settles assignments and bonuses there) and from configure()/resize().
    // Never during collection. Add/remove semantics are untouched.
    static void refreshForInit(CraftPilotsState& s);
private:
    // Assigned-pilot rows over live native list state. A private static
    // member (not a namespace-scope helper) so state friendship covers the
    // private widget reads; friendship is not transitive to free functions.
    // The live-text helper stays free: it takes an explicit widget pointer
    // and reads no private state itself.
    static std::vector<CalypsoTabbedRow> craftPilotsRows(const CraftPilotsState& state);
    // Live seat counts over native state (assigned rows, required seats).
    static void craftPilotsSeats(const CraftPilotsState& state, std::size_t& assigned, int& required);
    static void applyGeneratedLayout(CraftPilotsState& s, bool wide);
    static void positionWidgets(CraftPilotsState& s, bool wide);
    CraftPilotsState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
