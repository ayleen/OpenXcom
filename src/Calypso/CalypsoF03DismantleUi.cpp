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
#include <string>
namespace OpenXcom { namespace Calypso {
CalypsoF03DismantleUi::~CalypsoF03DismantleUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF03DismantleUi::topState() const { return _state; }

namespace {
CalypsoLogicalRect buttonVisualRect(const CalypsoF03DismantleGen::CalypsoF03DismantleGenLayout* g, int idx, const char* id, const CalypsoLogicalRect& winRect, double uiScale)
{
    if (!g || idx < 0 || idx >= CalypsoF03DismantleGen::kLayoutCount) return CalypsoLogicalRect{winRect.x, winRect.y, 0, 0};
    for (int i = 0; i < CalypsoF03DismantleGen::kButtonCount; ++i)
    {
        const auto &br = CalypsoF03DismantleGen::kButtonRects[idx][i];
        if (std::string(br.id) == id)
        {
            return CalypsoLogicalRect{ winRect.x + int((br.rect.x - g->window.x)*uiScale), winRect.y + int((br.rect.y - g->window.y)*uiScale), int(br.rect.w*uiScale), int(br.rect.h*uiScale) };
        }
    }
    return CalypsoLogicalRect{winRect.x, winRect.y, 0, 0};
}
}

void CalypsoF03DismantleUi::applyRects(DismantleFacilityState& s, bool wide)
{
    const auto* g = CalypsoF03DismantleGen::layoutForDesign(wide?1280:740, wide?720:360);
    if (!g) return;
    int idx = wide ? 0 : 1;
    if (s._window)
    {
        s._window->setX(g->window.x);
        s._window->setY(g->window.y);
        s._window->setWidth(g->window.w);
        s._window->setHeight(g->window.h);
    }
    if (idx < 0 || idx >= CalypsoF03DismantleGen::kLayoutCount) return;
    for (int i = 0; i < CalypsoF03DismantleGen::kButtonCount; ++i)
    {
        const auto &br = CalypsoF03DismantleGen::kButtonRects[idx][i];
        TextButton* btn = nullptr;
        if (std::string(br.id) == "cancel") btn = s._btnCancel;
        else if (std::string(br.id) == "confirm") btn = s._btnOk;
        else continue;
        if (btn)
        {
            btn->setX(br.rect.x);
            btn->setY(br.rect.y);
            btn->setWidth(br.rect.w);
            btn->setHeight(br.rect.h);
        }
    }
}

void CalypsoF03DismantleUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF03DismantleGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    int idx = wide ? 0 : 1;
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF03DismantleGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleWidget = _state->_txtTitle;
    m.titleText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
    m.messageWidget = _state->_txtFacility;
    m.messageText = _state->_txtFacility ? _state->_txtFacility->getText() : std::string();
    m.protocolText = std::string();
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
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF03DismantleGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF03DismantleGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnCancel; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = buttonVisualRect(g, idx, "cancel", winRect, uiScale); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF03DismantleGen::kButtonCount;++i) if(std::string(CalypsoF03DismantleGen::kButtons[i].id)=="cancel") { b.restFill=CalypsoF03DismantleGen::kButtons[i].fill; b.restBorder=CalypsoF03DismantleGen::kButtons[i].border; b.textColor=CalypsoF03DismantleGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = buttonVisualRect(g, idx, "confirm", winRect, uiScale); b.tone = CalypsoActionTone::Destructive; for(int i=0;i<CalypsoF03DismantleGen::kButtonCount;++i) if(std::string(CalypsoF03DismantleGen::kButtons[i].id)=="confirm") { b.restFill=CalypsoF03DismantleGen::kButtons[i].fill; b.restBorder=CalypsoF03DismantleGen::kButtons[i].border; b.textColor=CalypsoF03DismantleGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF03DismantleUi::configure(DismantleFacilityState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F03")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    applyRects(s, s._hdWideLayout);
    const auto* g = CalypsoF03DismantleGen::layoutForDesign(s._hdWideLayout?1280:740, s._hdWideLayout?720:360);
    if (g) s.enableUiScaling(g->designWidth, g->designHeight, 1.0f, false);
    else s.enableUiScaling(s._hdWideLayout?1280:740, s._hdWideLayout?720:360, 1.0f, false);
    auto* a = new CalypsoF03DismantleUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF03DismantleUi::resize(DismantleFacilityState& s) {
    if(!s._hdLayout) return false;
    bool wide = (Options::baseXResolution >= 1000);
    if (wide != s._hdWideLayout) {
        s._hdWideLayout = wide;
        applyRects(s, wide);
        const auto* g = CalypsoF03DismantleGen::layoutForDesign(wide?1280:740, wide?720:360);
        if (g) s.recaptureUiScaling(g->designWidth, g->designHeight, 1.0f, false);
        else s.recaptureUiScaling(wide?1280:740, wide?720:360, 1.0f, false);
    } else {
        applyRects(s, wide);
        s.applyUiScaling();
    }
    return true;
}
} }
#endif
