#ifdef __EMSCRIPTEN__
#include "CalypsoF22TrainingFinishedUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Geoscape/TrainingFinishedState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF22TrainingFinished.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF22TrainingFinishedUi::~CalypsoF22TrainingFinishedUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF22TrainingFinishedUi::topState() const { return _state; }
void CalypsoF22TrainingFinishedUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF22TrainingFinishedGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF22TrainingFinishedGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleText = m.messageText; // use message as title fallback
    m.messageWidget = _state->_txtTitle;
    m.messageText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
    m.protocolText = std::string();
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF22TrainingFinishedGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF22TrainingFinishedGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF22TrainingFinishedGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF22TrainingFinishedGen::kPanelFillBottom;
    m.frameColor = CalypsoF22TrainingFinishedGen::kFrame;
    m.protocolColor = CalypsoF22TrainingFinishedGen::kProtocolText;
    m.dividerColor = CalypsoF22TrainingFinishedGen::kDivider;
    m.footerDotColor = CalypsoF22TrainingFinishedGen::kFooterDot;
    m.warningColor = CalypsoF22TrainingFinishedGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF22TrainingFinishedGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF22TrainingFinishedGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF22TrainingFinishedGen::kButtonCount;++i) if(std::string(CalypsoF22TrainingFinishedGen::kButtons[i].id)=="ok") { b.restFill=CalypsoF22TrainingFinishedGen::kButtons[i].fill; b.restBorder=CalypsoF22TrainingFinishedGen::kButtons[i].border; b.textColor=CalypsoF22TrainingFinishedGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOpen; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF22TrainingFinishedGen::kButtonCount;++i) if(std::string(CalypsoF22TrainingFinishedGen::kButtons[i].id)=="open") { b.restFill=CalypsoF22TrainingFinishedGen::kButtons[i].fill; b.restBorder=CalypsoF22TrainingFinishedGen::kButtons[i].border; b.textColor=CalypsoF22TrainingFinishedGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF22TrainingFinishedUi::configure(TrainingFinishedState& s, bool allow) {
    // Disabled: complex screen would hide list/table data or is full-screen — keep legacy until proper archetype.
    s._hdLayout = false;
    return;

    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F22")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    // Canonical content-sized window: sync vanilla Window to generated rect for 1:1 projection
    { bool wide = s._hdWideLayout; const auto* g = CalypsoF22TrainingFinishedGen::layoutForDesign(wide?1280:740, wide?720:360); if (g) { s._window->setX(g->window.x); s._window->setY(g->window.y); s._window->setWidth(g->window.w); s._window->setHeight(g->window.h); } }
    auto* a = new CalypsoF22TrainingFinishedUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF22TrainingFinishedUi::resize(TrainingFinishedState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
