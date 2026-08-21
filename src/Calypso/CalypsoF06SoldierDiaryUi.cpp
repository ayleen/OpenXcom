#ifdef __EMSCRIPTEN__
#include "CalypsoF06SoldierDiaryUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Basescape/SoldierDiaryOverviewState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF06SoldierDiary.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF06SoldierDiaryUi::~CalypsoF06SoldierDiaryUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF06SoldierDiaryUi::topState() const { return _state; }
void CalypsoF06SoldierDiaryUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF06SoldierDiaryGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF06SoldierDiaryGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleText = m.messageText; // use message as title fallback
    m.messageWidget = _state->_txtTitle;
    m.messageText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
    m.protocolText = std::string();
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF06SoldierDiaryGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF06SoldierDiaryGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF06SoldierDiaryGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF06SoldierDiaryGen::kPanelFillBottom;
    m.frameColor = CalypsoF06SoldierDiaryGen::kFrame;
    m.protocolColor = CalypsoF06SoldierDiaryGen::kProtocolText;
    m.dividerColor = CalypsoF06SoldierDiaryGen::kDivider;
    m.footerDotColor = CalypsoF06SoldierDiaryGen::kFooterDot;
    m.warningColor = CalypsoF06SoldierDiaryGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF06SoldierDiaryGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF06SoldierDiaryGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF06SoldierDiaryGen::kButtonCount;++i) if(std::string(CalypsoF06SoldierDiaryGen::kButtons[i].id)=="ok") { b.restFill=CalypsoF06SoldierDiaryGen::kButtons[i].fill; b.restBorder=CalypsoF06SoldierDiaryGen::kButtons[i].border; b.textColor=CalypsoF06SoldierDiaryGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF06SoldierDiaryUi::configure(SoldierDiaryOverviewState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F06")) { s._hdLayout=false; return; }
    // F05/F06 are full screens, keep legacy (review blocker 2)
    s._hdLayout = false; return;
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    // Canonical content-sized window: sync vanilla Window to generated rect for 1:1 projection
    { bool wide = s._hdWideLayout; const auto* g = CalypsoF06SoldierDiaryGen::layoutForDesign(wide?1280:740, wide?720:360); if (g) { s._window->setX(g->window.x); s._window->setY(g->window.y); s._window->setWidth(g->window.w); s._window->setHeight(g->window.h); } }
    auto* a = new CalypsoF06SoldierDiaryUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF06SoldierDiaryUi::resize(SoldierDiaryOverviewState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
