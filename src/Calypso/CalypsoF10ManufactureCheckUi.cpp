#ifdef __EMSCRIPTEN__
#include "CalypsoF10ManufactureCheckUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Basescape/ManufactureInfoState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF10ManufactureCheck.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF10ManufactureCheckUi::~CalypsoF10ManufactureCheckUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF10ManufactureCheckUi::topState() const { return _state; }
void CalypsoF10ManufactureCheckUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF10ManufactureCheckGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF10ManufactureCheckGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleText = m.messageText; // use message as title fallback
    m.messageWidget = _state->_txtTitle;
    m.messageText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
    m.protocolText = std::string();
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF10ManufactureCheckGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF10ManufactureCheckGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF10ManufactureCheckGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF10ManufactureCheckGen::kPanelFillBottom;
    m.frameColor = CalypsoF10ManufactureCheckGen::kFrame;
    m.protocolColor = CalypsoF10ManufactureCheckGen::kProtocolText;
    m.dividerColor = CalypsoF10ManufactureCheckGen::kDivider;
    m.footerDotColor = CalypsoF10ManufactureCheckGen::kFooterDot;
    m.warningColor = CalypsoF10ManufactureCheckGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF10ManufactureCheckGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF10ManufactureCheckGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF10ManufactureCheckGen::kButtonCount;++i) if(std::string(CalypsoF10ManufactureCheckGen::kButtons[i].id)=="ok") { b.restFill=CalypsoF10ManufactureCheckGen::kButtons[i].fill; b.restBorder=CalypsoF10ManufactureCheckGen::kButtons[i].border; b.textColor=CalypsoF10ManufactureCheckGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnStop; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF10ManufactureCheckGen::kButtonCount;++i) if(std::string(CalypsoF10ManufactureCheckGen::kButtons[i].id)=="stop") { b.restFill=CalypsoF10ManufactureCheckGen::kButtons[i].fill; b.restBorder=CalypsoF10ManufactureCheckGen::kButtons[i].border; b.textColor=CalypsoF10ManufactureCheckGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF10ManufactureCheckUi::configure(ManufactureInfoState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F10")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    auto* a = new CalypsoF10ManufactureCheckUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF10ManufactureCheckUi::resize(ManufactureInfoState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
