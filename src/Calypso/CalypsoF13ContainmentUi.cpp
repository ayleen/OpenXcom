#ifdef __EMSCRIPTEN__
#include "CalypsoF13ContainmentUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Basescape/ManageAlienContainmentState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF13Containment.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF13ContainmentUi::~CalypsoF13ContainmentUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF13ContainmentUi::topState() const { return _state; }
void CalypsoF13ContainmentUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF13ContainmentGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF13ContainmentGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleText = m.messageText; // use message as title fallback
    m.messageWidget = _state->_txtTitle;
    m.messageText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
    m.protocolText = std::string();
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF13ContainmentGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF13ContainmentGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF13ContainmentGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF13ContainmentGen::kPanelFillBottom;
    m.frameColor = CalypsoF13ContainmentGen::kFrame;
    m.protocolColor = CalypsoF13ContainmentGen::kProtocolText;
    m.dividerColor = CalypsoF13ContainmentGen::kDivider;
    m.footerDotColor = CalypsoF13ContainmentGen::kFooterDot;
    m.warningColor = CalypsoF13ContainmentGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF13ContainmentGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF13ContainmentGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF13ContainmentGen::kButtonCount;++i) if(std::string(CalypsoF13ContainmentGen::kButtons[i].id)=="ok") { b.restFill=CalypsoF13ContainmentGen::kButtons[i].fill; b.restBorder=CalypsoF13ContainmentGen::kButtons[i].border; b.textColor=CalypsoF13ContainmentGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnCancel; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF13ContainmentGen::kButtonCount;++i) if(std::string(CalypsoF13ContainmentGen::kButtons[i].id)=="cancel") { b.restFill=CalypsoF13ContainmentGen::kButtons[i].fill; b.restBorder=CalypsoF13ContainmentGen::kButtons[i].border; b.textColor=CalypsoF13ContainmentGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF13ContainmentUi::configure(ManageAlienContainmentState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F13")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    auto* a = new CalypsoF13ContainmentUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF13ContainmentUi::resize(ManageAlienContainmentState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
