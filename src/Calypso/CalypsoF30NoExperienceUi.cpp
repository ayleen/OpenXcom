#ifdef __EMSCRIPTEN__
#include "CalypsoF30NoExperienceUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Battlescape/NoExperienceState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF30NoExperience.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF30NoExperienceUi::~CalypsoF30NoExperienceUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF30NoExperienceUi::topState() const { return _state; }
void CalypsoF30NoExperienceUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF30NoExperienceGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF30NoExperienceGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleText = m.messageText; // use message as title fallback
    m.messageWidget = _state->_txtTitle;
    m.messageText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
    m.protocolText = std::string();
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF30NoExperienceGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF30NoExperienceGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF30NoExperienceGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF30NoExperienceGen::kPanelFillBottom;
    m.frameColor = CalypsoF30NoExperienceGen::kFrame;
    m.protocolColor = CalypsoF30NoExperienceGen::kProtocolText;
    m.dividerColor = CalypsoF30NoExperienceGen::kDivider;
    m.footerDotColor = CalypsoF30NoExperienceGen::kFooterDot;
    m.warningColor = CalypsoF30NoExperienceGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF30NoExperienceGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF30NoExperienceGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnCancel; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF30NoExperienceGen::kButtonCount;++i) if(std::string(CalypsoF30NoExperienceGen::kButtons[i].id)=="ok") { b.restFill=CalypsoF30NoExperienceGen::kButtons[i].fill; b.restBorder=CalypsoF30NoExperienceGen::kButtons[i].border; b.textColor=CalypsoF30NoExperienceGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF30NoExperienceUi::configure(NoExperienceState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F30")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    auto* a = new CalypsoF30NoExperienceUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF30NoExperienceUi::resize(NoExperienceState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
