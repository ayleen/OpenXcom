#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class UfoDetectedState; namespace Calypso { class CalypsoF17UfoDetectedUi : public CalypsoHdFamilyAdapter {
public:
    /// Visual composition variant; purely presentational and owned by this
    /// adapter -- the game state never learns a layout exists.
    enum class LayoutVariant { Wide, Compact, Portrait };
    explicit CalypsoF17UfoDetectedUi(UfoDetectedState* s,
        LayoutVariant layout = LayoutVariant::Compact);
    ~CalypsoF17UfoDetectedUi() override;
    const void* topState() const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    void setLayout(LayoutVariant layout) { _layout = layout; }
    static void configure(UfoDetectedState& s, bool allow=true);
    static bool resize(UfoDetectedState& s);
private:
    static LayoutVariant chooseLayout();
    static void applyLayout(UfoDetectedState& s, LayoutVariant layout);
    static int layoutIndex(LayoutVariant layout);
    static int designWidth(LayoutVariant layout);
    static int designHeight(LayoutVariant layout);
    UfoDetectedState* _state=nullptr;
    LayoutVariant _layout = LayoutVariant::Compact;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
