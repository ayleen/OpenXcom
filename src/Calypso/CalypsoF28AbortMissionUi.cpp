#ifdef __EMSCRIPTEN__
#include "CalypsoF28AbortMissionUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Battlescape/AbortMissionState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF28AbortMission.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoHdOperationsChrome.h"
namespace OpenXcom { namespace Calypso {
CalypsoF28AbortMissionUi::~CalypsoF28AbortMissionUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF28AbortMissionUi::topState() const { return _state; }
void CalypsoF28AbortMissionUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF28AbortMissionGen::layoutForDesign(wide?1280:740, wide?720:360);
    if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Mission confirmation layout is unavailable");
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF28AbortMissionGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleWidget = _state->_txtAbort;
    m.titleText = _state->_txtAbort->getText();
    m.messageWidget = _state->_txtOutside;
    for (Text *line : {_state->_txtInEntrance, _state->_txtInExit, _state->_txtOutside}) {
        if (!line->getVisible()) continue;
        if (!m.messageText.empty()) m.messageText += "\n";
        m.messageText += line->getText();
    }
    m.uiScale = uiScale;
    const auto &metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
    m.projectionScaleX = uiScale * metrics.scaleX;
    m.projectionScaleY = uiScale * metrics.scaleY;
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF28AbortMissionGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF28AbortMissionGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF28AbortMissionGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF28AbortMissionGen::kPanelFillBottom;
    m.frameColor = CalypsoF28AbortMissionGen::kFrame;
    m.protocolColor = CalypsoF28AbortMissionGen::kProtocolText;
    m.dividerColor = CalypsoF28AbortMissionGen::kDivider;
    m.footerDotColor = CalypsoF28AbortMissionGen::kFooterDot;
    m.warningColor = CalypsoF28AbortMissionGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF28AbortMissionGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF28AbortMissionGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnCancel; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF28AbortMissionGen::kButtonCount;++i) if(std::string(CalypsoF28AbortMissionGen::kButtons[i].id)=="cancel") { b.restFill=CalypsoF28AbortMissionGen::kButtons[i].fill; b.restBorder=CalypsoF28AbortMissionGen::kButtons[i].border; b.textColor=CalypsoF28AbortMissionGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    if (_state->_btnOk->getVisible()) { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Destructive; for(int i=0;i<CalypsoF28AbortMissionGen::kButtonCount;++i) if(std::string(CalypsoF28AbortMissionGen::kButtons[i].id)=="confirm") { b.restFill=CalypsoF28AbortMissionGen::kButtons[i].fill; b.restBorder=CalypsoF28AbortMissionGen::kButtons[i].border; b.textColor=CalypsoF28AbortMissionGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF28AbortMissionUi::configure(AbortMissionState &s, bool allow)
{
	if (!allow || !s._game || !s._battleGame)
		CalypsoHdUiOverlay::instance().failHdRoute("Mission confirmation is unavailable");
	s._screen = true;
	s._hdLayout = true;
	resize(s);
	s._hdAdapter = new CalypsoF28AbortMissionUi(&s);
	CalypsoHdUiOverlay::instance().registerAdapter(s._hdAdapter);
}

bool CalypsoF28AbortMissionUi::resize(AbortMissionState &s)
{
	if (!s._hdLayout) return false;
	const auto viewport = calypsoHdOperationsEffectiveViewport();
	const bool wide = viewport.cssWidth >= 1000 && viewport.cssHeight >= 550;
	s._hdWideLayout = wide;
	namespace G = CalypsoF28AbortMissionGen;
	const auto *g = G::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Mission confirmation layout is unavailable");
	auto place = [](Surface *widget, const auto &rect) {
		widget->setX(rect.x);
		widget->setY(rect.y);
		widget->setWidth(rect.w);
		widget->setHeight(rect.h);
	};
	place(s._window, g->window);
	place(s._txtAbort, g->title);
	place(s._txtOutside, g->message);
	for (const auto &button : G::kButtonRects[wide ? 0 : 1])
		place(std::string(button.id) == "cancel" ? s._btnCancel : s._btnOk, button.rect);
	if (s._hdAdapter) s.recaptureUiScaling(g->designWidth, g->designHeight, 1.0f, false);
	else s.enableUiScaling(g->designWidth, g->designHeight, 1.0f, false);
	return true;
}
} }
#endif
