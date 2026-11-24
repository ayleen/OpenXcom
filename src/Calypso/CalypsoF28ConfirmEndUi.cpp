#ifdef __EMSCRIPTEN__
#include "CalypsoF28ConfirmEndUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Battlescape/ConfirmEndMissionState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF28ConfirmEnd.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoHdOperationsChrome.h"
namespace OpenXcom { namespace Calypso {
CalypsoF28ConfirmEndUi::~CalypsoF28ConfirmEndUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF28ConfirmEndUi::topState() const { return _state; }
void CalypsoF28ConfirmEndUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF28ConfirmEndGen::layoutForDesign(wide?1280:740, wide?720:360);
    if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Mission confirmation layout is unavailable");
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF28ConfirmEndGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleWidget = _state->_txtTitle;
    m.titleText = _state->_txtTitle->getText();
    m.messageWidget = _state->_txtConfirm;
    m.messageText = _state->_txtWounded->getText() + "\n" + _state->_txtConfirm->getText();
    m.uiScale = uiScale;
    const auto &metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
    m.projectionScaleX = uiScale * metrics.scaleX;
    m.projectionScaleY = uiScale * metrics.scaleY;
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF28ConfirmEndGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF28ConfirmEndGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF28ConfirmEndGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF28ConfirmEndGen::kPanelFillBottom;
    m.frameColor = CalypsoF28ConfirmEndGen::kFrame;
    m.protocolColor = CalypsoF28ConfirmEndGen::kProtocolText;
    m.dividerColor = CalypsoF28ConfirmEndGen::kDivider;
    m.footerDotColor = CalypsoF28ConfirmEndGen::kFooterDot;
    m.warningColor = CalypsoF28ConfirmEndGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF28ConfirmEndGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF28ConfirmEndGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnCancel; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF28ConfirmEndGen::kButtonCount;++i) if(std::string(CalypsoF28ConfirmEndGen::kButtons[i].id)=="cancel") { b.restFill=CalypsoF28ConfirmEndGen::kButtons[i].fill; b.restBorder=CalypsoF28ConfirmEndGen::kButtons[i].border; b.textColor=CalypsoF28ConfirmEndGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    if (_state->_btnOk->getVisible()) { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnOk; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF28ConfirmEndGen::kButtonCount;++i) if(std::string(CalypsoF28ConfirmEndGen::kButtons[i].id)=="confirm") { b.restFill=CalypsoF28ConfirmEndGen::kButtons[i].fill; b.restBorder=CalypsoF28ConfirmEndGen::kButtons[i].border; b.textColor=CalypsoF28ConfirmEndGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF28ConfirmEndUi::configure(ConfirmEndMissionState &s, bool allow)
{
	if (!allow || !s._game || !s._battleGame)
		CalypsoHdUiOverlay::instance().failHdRoute("Mission confirmation is unavailable");
	s._screen = true;
	s._hdLayout = true;
	resize(s);
	s._hdAdapter = new CalypsoF28ConfirmEndUi(&s);
	CalypsoHdUiOverlay::instance().registerAdapter(s._hdAdapter);
}

bool CalypsoF28ConfirmEndUi::resize(ConfirmEndMissionState &s)
{
	if (!s._hdLayout) return false;
	const auto viewport = calypsoHdOperationsEffectiveViewport();
	const bool wide = viewport.cssWidth >= 1000 && viewport.cssHeight >= 550;
	s._hdWideLayout = wide;
	namespace G = CalypsoF28ConfirmEndGen;
	const auto *g = G::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Mission confirmation layout is unavailable");
	auto place = [](Surface *widget, const auto &rect) {
		widget->setX(rect.x);
		widget->setY(rect.y);
		widget->setWidth(rect.w);
		widget->setHeight(rect.h);
	};
	place(s._window, g->window);
	place(s._txtTitle, g->title);
	place(s._txtConfirm, g->message);
	for (const auto &button : G::kButtonRects[wide ? 0 : 1])
		place(std::string(button.id) == "cancel" ? s._btnCancel : s._btnOk, button.rect);
	if (s._hdAdapter) s.recaptureUiScaling(g->designWidth, g->designHeight, 1.0f, false);
	else s.enableUiScaling(g->designWidth, g->designHeight, 1.0f, false);
	return true;
}
} }
#endif
