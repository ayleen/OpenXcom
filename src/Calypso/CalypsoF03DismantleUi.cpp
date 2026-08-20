#ifdef __EMSCRIPTEN__
#include "CalypsoF03DismantleUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Basescape/DismantleFacilityState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF03Dismantle.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF03DismantleUi::~CalypsoF03DismantleUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF03DismantleUi::topState() const { return _state; }
void CalypsoF03DismantleUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF03DismantleGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF03DismantleGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.messageWidget = _state->_txtFacility;
    m.messageText = _state->_txtFacility ? _state->_txtFacility->getText() : std::string();
    m.titleText = "";
    m.protocolText = "";
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF03DismantleGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF03DismantleGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF03DismantleGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF03DismantleGen::kPanelFillBottom;
    m.frameColor = CalypsoF03DismantleGen::kFrame;
    m.protocolColor = CalypsoF03DismantleGen::kProtocolText;
    m.dividerColor = CalypsoF03DismantleGen::kDivider;
    m.footerDotColor = CalypsoF03DismantleGen::kFooterDot;
    m.warningColor = CalypsoF03DismantleGen::kWarning;

    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnCancel; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Destructive; m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF03DismantleUi::configure(DismantleFacilityState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F03")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    auto* a = new CalypsoF03DismantleUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF03DismantleUi::resize(DismantleFacilityState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
