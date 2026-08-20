#ifdef __EMSCRIPTEN__
#include "CalypsoF20ConfirmCydoniaUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Geoscape/ConfirmCydoniaState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF20ConfirmCydonia.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF20ConfirmCydoniaUi::~CalypsoF20ConfirmCydoniaUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF20ConfirmCydoniaUi::topState() const { return _state; }
void CalypsoF20ConfirmCydoniaUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF20ConfirmCydoniaGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF20ConfirmCydoniaGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleText = m.messageText; // use message as title fallback
    m.messageWidget = _state->_txtMessage;
    m.messageText = _state->_txtMessage ? _state->_txtMessage->getText() : std::string();
    m.protocolText = std::string();
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF20ConfirmCydoniaGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF20ConfirmCydoniaGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF20ConfirmCydoniaGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF20ConfirmCydoniaGen::kPanelFillBottom;
    m.frameColor = CalypsoF20ConfirmCydoniaGen::kFrame;
    m.protocolColor = CalypsoF20ConfirmCydoniaGen::kProtocolText;
    m.dividerColor = CalypsoF20ConfirmCydoniaGen::kDivider;
    m.footerDotColor = CalypsoF20ConfirmCydoniaGen::kFooterDot;
    m.warningColor = CalypsoF20ConfirmCydoniaGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF20ConfirmCydoniaGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF20ConfirmCydoniaGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnNo; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF20ConfirmCydoniaGen::kButtonCount;++i) if(std::string(CalypsoF20ConfirmCydoniaGen::kButtons[i].id)=="cancel") { b.restFill=CalypsoF20ConfirmCydoniaGen::kButtons[i].fill; b.restBorder=CalypsoF20ConfirmCydoniaGen::kButtons[i].border; b.textColor=CalypsoF20ConfirmCydoniaGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnYes; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Destructive; for(int i=0;i<CalypsoF20ConfirmCydoniaGen::kButtonCount;++i) if(std::string(CalypsoF20ConfirmCydoniaGen::kButtons[i].id)=="confirm") { b.restFill=CalypsoF20ConfirmCydoniaGen::kButtons[i].fill; b.restBorder=CalypsoF20ConfirmCydoniaGen::kButtons[i].border; b.textColor=CalypsoF20ConfirmCydoniaGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF20ConfirmCydoniaUi::configure(ConfirmCydoniaState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F20")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    auto* a = new CalypsoF20ConfirmCydoniaUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF20ConfirmCydoniaUi::resize(ConfirmCydoniaState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
