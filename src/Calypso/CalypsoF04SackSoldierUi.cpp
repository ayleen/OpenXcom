#ifdef __EMSCRIPTEN__
#include "CalypsoF04SackSoldierUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Basescape/SackSoldierState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF04SackSoldier.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF04SackSoldierUi::~CalypsoF04SackSoldierUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF04SackSoldierUi::topState() const { return _state; }
void CalypsoF04SackSoldierUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF04SackSoldierGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF04SackSoldierGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleWidget = _state->_txtTitle;
    m.titleText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
    m.messageWidget = _state->_txtSoldier;
    m.messageText = _state->_txtSoldier ? _state->_txtSoldier->getText() : std::string();
    m.protocolText = std::string();
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF04SackSoldierGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF04SackSoldierGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF04SackSoldierGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF04SackSoldierGen::kPanelFillBottom;
    m.frameColor = CalypsoF04SackSoldierGen::kFrame;
    m.protocolColor = CalypsoF04SackSoldierGen::kProtocolText;
    m.dividerColor = CalypsoF04SackSoldierGen::kDivider;
    m.footerDotColor = CalypsoF04SackSoldierGen::kFooterDot;
    m.warningColor = CalypsoF04SackSoldierGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF04SackSoldierGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF04SackSoldierGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnCancel; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF04SackSoldierGen::kButtonCount;++i) if(std::string(CalypsoF04SackSoldierGen::kButtons[i].id)=="cancel") { b.restFill=CalypsoF04SackSoldierGen::kButtons[i].fill; b.restBorder=CalypsoF04SackSoldierGen::kButtons[i].border; b.textColor=CalypsoF04SackSoldierGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Destructive; for(int i=0;i<CalypsoF04SackSoldierGen::kButtonCount;++i) if(std::string(CalypsoF04SackSoldierGen::kButtons[i].id)=="confirm") { b.restFill=CalypsoF04SackSoldierGen::kButtons[i].fill; b.restBorder=CalypsoF04SackSoldierGen::kButtons[i].border; b.textColor=CalypsoF04SackSoldierGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF04SackSoldierUi::configure(SackSoldierState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F04")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    // Canonical content-sized window: sync vanilla Window to generated rect for 1:1 projection
    { bool wide = s._hdWideLayout; const auto* g = CalypsoF04SackSoldierGen::layoutForDesign(wide?1280:740, wide?720:360); if (g) { s._window->setX(g->window.x); s._window->setY(g->window.y); s._window->setWidth(g->window.w); s._window->setHeight(g->window.h); } }
    auto* a = new CalypsoF04SackSoldierUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF04SackSoldierUi::resize(SackSoldierState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
