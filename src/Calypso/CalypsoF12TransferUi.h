#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class TransferBaseState; class TransferItemsState; namespace Calypso {
class CalypsoF12TransferBaseUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF12TransferBaseUi(TransferBaseState* s) : _state(s) {}
    ~CalypsoF12TransferBaseUi() override;
    const void* topState() const override;
    bool suppressLogicalState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(TransferBaseState& s);
    static void teardown(TransferBaseState& s);
    static bool resize(TransferBaseState& s);
private:
    static void applyGeneratedLayout(TransferBaseState& s, bool wide);
    TransferBaseState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
};
class CalypsoF12TransferItemsUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF12TransferItemsUi(TransferItemsState* s) : _state(s) {}
    ~CalypsoF12TransferItemsUi() override;
    const void* topState() const override;
    bool suppressLogicalState() const override;
    void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(TransferItemsState& s);
    static void teardown(TransferItemsState& s);
    static bool resize(TransferItemsState& s);
private:
    static void applyGeneratedLayout(TransferItemsState& s, bool wide);
    TransferItemsState* _state=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
};
} }
#endif
