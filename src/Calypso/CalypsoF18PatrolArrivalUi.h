#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom { class CraftPatrolState; namespace Calypso { class CalypsoF18PatrolArrivalUi : public CalypsoHdFamilyAdapter {
public:
    explicit CalypsoF18PatrolArrivalUi(CraftPatrolState* s) : _state(s) {}
    ~CalypsoF18PatrolArrivalUi() override;
    const void* topState() const override;
    const void* physicalUnderlayState() const override { return _underlay; }
    void collect(CalypsoHdFrameBuilder& b) const override;
    static void configure(CraftPatrolState& s, bool allow=true);
    static bool resize(CraftPatrolState& s);
private:
    CraftPatrolState* _state=nullptr;
    const void* _underlay=nullptr;
    mutable CalypsoSmallConfirmationMotion _motion;
}; } }
#endif
