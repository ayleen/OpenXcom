#ifdef __EMSCRIPTEN__
#include "CalypsoF18NotEnoughPilotsUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Geoscape/CraftNotEnoughPilotsState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF18NotEnoughPilots.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF18NotEnoughPilotsUi::~CalypsoF18NotEnoughPilotsUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF18NotEnoughPilotsUi::topState() const { return _state; }
void CalypsoF18NotEnoughPilotsUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF18NotEnoughPilotsGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF18NotEnoughPilotsGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleText = m.messageText; // use message as title fallback
    m.messageWidget = _state->_txtMessage;
    m.messageText = _state->_txtMessage ? _state->_txtMessage->getText() : std::string();
    m.protocolText = std::string();
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF18NotEnoughPilotsGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF18NotEnoughPilotsGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF18NotEnoughPilotsGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF18NotEnoughPilotsGen::kPanelFillBottom;
    m.frameColor = CalypsoF18NotEnoughPilotsGen::kFrame;
    m.protocolColor = CalypsoF18NotEnoughPilotsGen::kProtocolText;
    m.dividerColor = CalypsoF18NotEnoughPilotsGen::kDivider;
    m.footerDotColor = CalypsoF18NotEnoughPilotsGen::kFooterDot;
    m.warningColor = CalypsoF18NotEnoughPilotsGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF18NotEnoughPilotsGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF18NotEnoughPilotsGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF18NotEnoughPilotsGen::kButtonCount;++i) if(std::string(CalypsoF18NotEnoughPilotsGen::kButtons[i].id)=="ok") { b.restFill=CalypsoF18NotEnoughPilotsGen::kButtons[i].fill; b.restBorder=CalypsoF18NotEnoughPilotsGen::kButtons[i].border; b.textColor=CalypsoF18NotEnoughPilotsGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnAssignPilots; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF18NotEnoughPilotsGen::kButtonCount;++i) if(std::string(CalypsoF18NotEnoughPilotsGen::kButtons[i].id)=="assign") { b.restFill=CalypsoF18NotEnoughPilotsGen::kButtons[i].fill; b.restBorder=CalypsoF18NotEnoughPilotsGen::kButtons[i].border; b.textColor=CalypsoF18NotEnoughPilotsGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF18NotEnoughPilotsUi::configure(CraftNotEnoughPilotsState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F18")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    auto* a = new CalypsoF18NotEnoughPilotsUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF18NotEnoughPilotsUi::resize(CraftNotEnoughPilotsState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
