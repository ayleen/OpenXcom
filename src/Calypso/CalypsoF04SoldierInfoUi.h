#pragma once
#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class SoldierInfoState; class TextList; namespace Calypso { class CalypsoF04SoldierInfoUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF04SoldierInfoUi(SoldierInfoState* s) : _state(s) {}
    ~CalypsoF04SoldierInfoUi() override;
    const void* topState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    // Covered ownership: profile chrome must stay hidden under pushed children
    // (armor, diary, bonus, rank, sack) instead of leaking vanilla.
    bool suppressWhenCovered() const override { return true; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SoldierInfoState& s);
    static bool resize(SoldierInfoState& s);
    // Rebuilds the inspector list from live native stat texts. Called from the
    // state's init() tail (values/visibility settle there) and from
    // configure(); never during collection.
    static void refreshInspectorRows(SoldierInfoState& s);
private:
    // Inspector collector over live native stat texts. A private static
    // member (not a namespace-scope helper) so state friendship covers the
    // private widget reads; friendship is not transitive to free functions.
    // The single-row/pair push helpers stay free: they take explicit widget
    // pointers and read no private state themselves.
    static std::vector<std::string> inspectorRows(const SoldierInfoState& state);
    static void applyGeneratedLayout(SoldierInfoState& s, bool wide);
    SoldierInfoState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
