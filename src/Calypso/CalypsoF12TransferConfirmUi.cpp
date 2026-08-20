#ifdef __EMSCRIPTEN__
#include "CalypsoF12TransferConfirmUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Basescape/TransferConfirmState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF12TransferConfirm.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF12TransferConfirmUi::~CalypsoF12TransferConfirmUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF12TransferConfirmUi::topState() const { return _state; }
void CalypsoF12TransferConfirmUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF12TransferConfirmGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF12TransferConfirmGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleText = m.messageText; // use message as title fallback
    m.messageWidget = _state->_txtTitle;
    m.messageText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
    m.protocolText = std::string();
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF12TransferConfirmGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF12TransferConfirmGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF12TransferConfirmGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF12TransferConfirmGen::kPanelFillBottom;
    m.frameColor = CalypsoF12TransferConfirmGen::kFrame;
    m.protocolColor = CalypsoF12TransferConfirmGen::kProtocolText;
    m.dividerColor = CalypsoF12TransferConfirmGen::kDivider;
    m.footerDotColor = CalypsoF12TransferConfirmGen::kFooterDot;
    m.warningColor = CalypsoF12TransferConfirmGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF12TransferConfirmGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF12TransferConfirmGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnCancel; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF12TransferConfirmGen::kButtonCount;++i) if(std::string(CalypsoF12TransferConfirmGen::kButtons[i].id)=="cancel") { b.restFill=CalypsoF12TransferConfirmGen::kButtons[i].fill; b.restBorder=CalypsoF12TransferConfirmGen::kButtons[i].border; b.textColor=CalypsoF12TransferConfirmGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF12TransferConfirmGen::kButtonCount;++i) if(std::string(CalypsoF12TransferConfirmGen::kButtons[i].id)=="confirm") { b.restFill=CalypsoF12TransferConfirmGen::kButtons[i].fill; b.restBorder=CalypsoF12TransferConfirmGen::kButtons[i].border; b.textColor=CalypsoF12TransferConfirmGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF12TransferConfirmUi::configure(TransferConfirmState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F12")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    // Canonical content-sized window: sync vanilla Window to generated rect for 1:1 projection
    { bool wide = s._hdWideLayout; const auto* g = CalypsoF12TransferConfirmGen::layoutForDesign(wide?1280:740, wide?720:360); if (g) { s._window->setX(g->window.x); s._window->setY(g->window.y); s._window->setWidth(g->window.w); s._window->setHeight(g->window.h); } }
    auto* a = new CalypsoF12TransferConfirmUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF12TransferConfirmUi::resize(TransferConfirmState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
