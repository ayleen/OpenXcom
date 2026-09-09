#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class CalypsoMarketState; namespace Calypso {
class CalypsoF36MarketUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF36MarketUi(CalypsoMarketState* s) : _state(s) {}
    ~CalypsoF36MarketUi() override;
    const void* topState() const override;
    bool suppressLogicalState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CalypsoMarketState& s);
    static void teardown(CalypsoMarketState& s);
    static bool resize(CalypsoMarketState& s);
private:
    static void applyGeneratedLayout(CalypsoMarketState& s, bool wide);
    CalypsoMarketState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
};
} }
#endif
