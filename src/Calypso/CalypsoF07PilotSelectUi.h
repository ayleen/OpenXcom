#pragma once
#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoTabbedManagementRenderer.h"
namespace OpenXcom { class CraftPilotSelectState; namespace Calypso {
 // F07 pilot-candidate production adapter (shared tabbed-management
 // renderer). The native candidate list stays the behavior/input owner:
 // row click assigns immediately through the unchanged handler (candidates
 // are divers aboard the craft satisfying all piloting requirements and not
 // already assigned), Cancel pops non-mutating. No invented candidate pool.
 // The fixture Assign entry resolves to no native widget (assignment is the
 // row tap itself), so it stays unpainted instead of a dead-end control.
 class CalypsoF07PilotSelectUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF07PilotSelectUi(CraftPilotSelectState* s) : _state(s) {}
    ~CalypsoF07PilotSelectUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftPilotSelectState& s);
    static bool resize(CraftPilotSelectState& s);
private:
    // Row snapshot over live native list state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions.
    struct PilotSelectRows
    {
        std::vector<std::string> ids;
        std::vector<CalypsoTabbedRow> rows;
    };
    static PilotSelectRows pilotSelectRows(const CraftPilotSelectState& state);
    static void applyGeneratedLayout(CraftPilotSelectState& s, bool wide);
    CraftPilotSelectState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
