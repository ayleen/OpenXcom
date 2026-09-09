#pragma once
#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoTabbedManagementRenderer.h"
namespace OpenXcom { class CraftsState; namespace Calypso {
 // F07 submarine-roster production adapter (shared tabbed-management
 // renderer). The native craft list stays the behavior/input owner: row
 // click opens the overview (Out craft cannot open), Shift+right-click
 // reorders, middle-click opens the craft article, Done keeps the
 // storage-overflow SellState/ErrorMessageState handoff. Out rows paint the
 // live status text with a disabled readout; no duplicate acquisition entry
 // is invented.
 class CalypsoF07CraftsUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF07CraftsUi(CraftsState* s) : _state(s) {}
    ~CalypsoF07CraftsUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: roster chrome must stay hidden under pushed children
    // (overview, Sell, ErrorMessage) instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftsState& s);
    static bool resize(CraftsState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct CraftRows
    {
        std::vector<std::string> ids;
        std::vector<CalypsoTabbedRow> rows;
    };
    static CraftRows craftRows(const CraftsState& state);
    static void applyGeneratedLayout(CraftsState& s, bool wide);
    CraftsState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
