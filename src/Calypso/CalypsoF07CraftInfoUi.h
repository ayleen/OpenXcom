#pragma once
#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoTabbedManagementRenderer.h"
namespace OpenXcom { class CraftInfoState; class TextButton; class TextList; namespace Calypso {
 // F07 submarine-overview production adapter (shared tabbed-management
 // renderer). Paints the live craft name, named capability tabs, weapon-mount
 // rows, summary facts, the selected-mount inspector, and footer from native
 // state every frame; every painted control resolves to its existing native
 // widget and handler: name editing (_edtCraft over the painted title),
 // numbered mount buttons (Change route, with the lead mount's button
 // re-seated on its inspector slot), weapon icons (enable toggle on click,
 // article on middle-click), capability tabs (Crew/Equipment/Armor/Pilots
 // routes), and Done. Native state names no selected mount, so the inspector
 // follows the lead mount row every frame; its row paints disabled
 // display-only while the icon zone keeps the toggle/article handler live
 // (fixed-weapon precedent). Capability visibility is read from the native
 // buttons each frame, so Ketos-like zero-capacity craft paint the overview
 // tab only. Controls without a contract slot (craft skin toggle, debug-only
 // New Battle, the rename/more fixture entries with no native widget) stay
 // live parked behavior owners without painted controls or pointer hit
 // areas; the inspector Reference entry likewise paints nothing, since no
 // native tap owner exists for it (article stays middle-click plus the
 // keyboard shortcut).
 class CalypsoF07CraftInfoUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF07CraftInfoUi(CraftInfoState* s) : _state(s) {}
    ~CalypsoF07CraftInfoUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: overview chrome must stay hidden under pushed
    // children (crew/equipment/armor/weapons/pilots, Ufopaedia articles)
    // instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftInfoState& s);
    static bool resize(CraftInfoState& s);
    // Repositions action hit areas onto the current row plan. Called from the
    // state's init() tail (native init settles tab visibility and mount
    // texts there) and from configure()/resize(). Never during collection.
    // Row structure is rules-stable per craft; texts refresh every collect.
    static void refreshForInit(CraftInfoState& s);
private:
    // Mount-row plan over live native state. A private static member (not
    // a namespace-scope helper) so state friendship covers the private
    // widget reads; friendship is not transitive to free functions. The
    // text join/flatten/label helpers stay free: they take explicit values
    // and read no private state themselves.
    struct CraftInfoRow
    {
        // Weapon slot index.
        int slot = -1;
        std::string text;
        bool enabled = false;
    };
    static std::vector<CraftInfoRow> craftInfoPlan(const CraftInfoState& state);
    static void applyGeneratedLayout(CraftInfoState& s, bool wide);
    static void positionWidgets(CraftInfoState& s, bool wide);
    CraftInfoState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
