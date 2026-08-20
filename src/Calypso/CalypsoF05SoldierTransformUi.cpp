#ifdef __EMSCRIPTEN__
#include "CalypsoF05SoldierTransformUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Basescape/SoldierTransformState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF05SoldierTransform.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF05SoldierTransformUi::~CalypsoF05SoldierTransformUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF05SoldierTransformUi::topState() const { return _state; }
void CalypsoF05SoldierTransformUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF05SoldierTransformGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF05SoldierTransformGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleText = m.messageText; // use message as title fallback
    m.messageWidget = _state->_txtTitle;
    m.messageText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
    m.protocolText = std::string();
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF05SoldierTransformGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF05SoldierTransformGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF05SoldierTransformGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF05SoldierTransformGen::kPanelFillBottom;
    m.frameColor = CalypsoF05SoldierTransformGen::kFrame;
    m.protocolColor = CalypsoF05SoldierTransformGen::kProtocolText;
    m.dividerColor = CalypsoF05SoldierTransformGen::kDivider;
    m.footerDotColor = CalypsoF05SoldierTransformGen::kFooterDot;
    m.warningColor = CalypsoF05SoldierTransformGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF05SoldierTransformGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF05SoldierTransformGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnCancel; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF05SoldierTransformGen::kButtonCount;++i) if(std::string(CalypsoF05SoldierTransformGen::kButtons[i].id)=="cancel") { b.restFill=CalypsoF05SoldierTransformGen::kButtons[i].fill; b.restBorder=CalypsoF05SoldierTransformGen::kButtons[i].border; b.textColor=CalypsoF05SoldierTransformGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF05SoldierTransformUi::configure(SoldierTransformState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F05")) { s._hdLayout=false; return; }
    // F05/F06 are full screens, keep legacy (review blocker 2)
    s._hdLayout = false; return;
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    auto* a = new CalypsoF05SoldierTransformUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF05SoldierTransformUi::resize(SoldierTransformState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
