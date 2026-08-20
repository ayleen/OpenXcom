#ifdef __EMSCRIPTEN__
#include "CalypsoF19DogfightErrorUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Geoscape/DogfightErrorState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF19DogfightError.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF19DogfightErrorUi::~CalypsoF19DogfightErrorUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF19DogfightErrorUi::topState() const { return _state; }
void CalypsoF19DogfightErrorUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF19DogfightErrorGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF19DogfightErrorGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.messageWidget = _state->_txtMessage;
    m.messageText = _state->_txtMessage ? _state->_txtMessage->getText() : std::string();
    m.titleText = "";
    m.protocolText = "";
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF19DogfightErrorGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF19DogfightErrorGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF19DogfightErrorGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF19DogfightErrorGen::kPanelFillBottom;
    m.frameColor = CalypsoF19DogfightErrorGen::kFrame;
    m.protocolColor = CalypsoF19DogfightErrorGen::kProtocolText;
    m.dividerColor = CalypsoF19DogfightErrorGen::kDivider;
    m.footerDotColor = CalypsoF19DogfightErrorGen::kFooterDot;
    m.warningColor = CalypsoF19DogfightErrorGen::kWarning;

    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnIntercept; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnBase; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF19DogfightErrorUi::configure(DogfightErrorState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F19")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    auto* a = new CalypsoF19DogfightErrorUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF19DogfightErrorUi::resize(DogfightErrorState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
