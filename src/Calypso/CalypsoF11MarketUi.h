#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class PurchaseState; class SellState; namespace Calypso {
class CalypsoF11PurchaseUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF11PurchaseUi(PurchaseState* s) : _state(s) {}
    ~CalypsoF11PurchaseUi() override;
    const void* topState() const override;
    bool suppressLogicalState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(PurchaseState& s);
    static void teardown(PurchaseState& s);
    static bool resize(PurchaseState& s);
private:
    static void applyGeneratedLayout(PurchaseState& s, bool wide);
    PurchaseState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
};
class CalypsoF11SellUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF11SellUi(SellState* s) : _state(s) {}
    ~CalypsoF11SellUi() override;
    const void* topState() const override;
    bool suppressLogicalState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(SellState& s);
    static void teardown(SellState& s);
    static bool resize(SellState& s);
private:
    static void applyGeneratedLayout(SellState& s, bool wide);
    SellState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
};
} }
#endif
