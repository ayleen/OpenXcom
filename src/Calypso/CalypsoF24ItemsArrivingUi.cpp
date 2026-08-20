#ifdef __EMSCRIPTEN__
#include "CalypsoF24ItemsArrivingUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Geoscape/ItemsArrivingState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF24ItemsArriving.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF24ItemsArrivingUi::~CalypsoF24ItemsArrivingUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF24ItemsArrivingUi::topState() const { return _state; }
void CalypsoF24ItemsArrivingUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF24ItemsArrivingGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF24ItemsArrivingGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleText = m.messageText; // use message as title fallback
    m.messageWidget = _state->_txtTitle;
    m.messageText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
    m.protocolText = std::string();
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF24ItemsArrivingGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF24ItemsArrivingGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF24ItemsArrivingGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF24ItemsArrivingGen::kPanelFillBottom;
    m.frameColor = CalypsoF24ItemsArrivingGen::kFrame;
    m.protocolColor = CalypsoF24ItemsArrivingGen::kProtocolText;
    m.dividerColor = CalypsoF24ItemsArrivingGen::kDivider;
    m.footerDotColor = CalypsoF24ItemsArrivingGen::kFooterDot;
    m.warningColor = CalypsoF24ItemsArrivingGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF24ItemsArrivingGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF24ItemsArrivingGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF24ItemsArrivingGen::kButtonCount;++i) if(std::string(CalypsoF24ItemsArrivingGen::kButtons[i].id)=="ok") { b.restFill=CalypsoF24ItemsArrivingGen::kButtons[i].fill; b.restBorder=CalypsoF24ItemsArrivingGen::kButtons[i].border; b.textColor=CalypsoF24ItemsArrivingGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnGotoBase; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF24ItemsArrivingGen::kButtonCount;++i) if(std::string(CalypsoF24ItemsArrivingGen::kButtons[i].id)=="goto") { b.restFill=CalypsoF24ItemsArrivingGen::kButtons[i].fill; b.restBorder=CalypsoF24ItemsArrivingGen::kButtons[i].border; b.textColor=CalypsoF24ItemsArrivingGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF24ItemsArrivingUi::configure(ItemsArrivingState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F24")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    auto* a = new CalypsoF24ItemsArrivingUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF24ItemsArrivingUi::resize(ItemsArrivingState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
