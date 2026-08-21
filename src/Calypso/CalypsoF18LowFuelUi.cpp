#ifdef __EMSCRIPTEN__
#include "CalypsoF18LowFuelUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Geoscape/LowFuelState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF18LowFuel.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF18LowFuelUi::~CalypsoF18LowFuelUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF18LowFuelUi::topState() const { return _state; }
void CalypsoF18LowFuelUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF18LowFuelGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF18LowFuelGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleWidget = _state->_txtTitle;
    m.titleText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
    m.messageWidget = _state->_txtMessage;
    m.messageText = _state->_txtMessage ? _state->_txtMessage->getText() : std::string();
    m.protocolText = std::string();
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF18LowFuelGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF18LowFuelGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF18LowFuelGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF18LowFuelGen::kPanelFillBottom;
    m.frameColor = CalypsoF18LowFuelGen::kFrame;
    m.protocolColor = CalypsoF18LowFuelGen::kProtocolText;
    m.dividerColor = CalypsoF18LowFuelGen::kDivider;
    m.footerDotColor = CalypsoF18LowFuelGen::kFooterDot;
    m.warningColor = CalypsoF18LowFuelGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF18LowFuelGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF18LowFuelGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF18LowFuelGen::kButtonCount;++i) if(std::string(CalypsoF18LowFuelGen::kButtons[i].id)=="ok") { b.restFill=CalypsoF18LowFuelGen::kButtons[i].fill; b.restBorder=CalypsoF18LowFuelGen::kButtons[i].border; b.textColor=CalypsoF18LowFuelGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk5Secs; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF18LowFuelGen::kButtonCount;++i) if(std::string(CalypsoF18LowFuelGen::kButtons[i].id)=="ok5secs") { b.restFill=CalypsoF18LowFuelGen::kButtons[i].fill; b.restBorder=CalypsoF18LowFuelGen::kButtons[i].border; b.textColor=CalypsoF18LowFuelGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF18LowFuelUi::configure(LowFuelState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F18")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    // Canonical content-sized window: sync vanilla Window to generated rect for 1:1 projection
    { bool wide = s._hdWideLayout; const auto* g = CalypsoF18LowFuelGen::layoutForDesign(wide?1280:740, wide?720:360); if (g) { s._window->setX(g->window.x); s._window->setY(g->window.y); s._window->setWidth(g->window.w); s._window->setHeight(g->window.h); } }
    auto* a = new CalypsoF18LowFuelUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF18LowFuelUi::resize(LowFuelState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
