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
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdHarnessHostState.h"
namespace OpenXcom { namespace Calypso {
CalypsoF19DogfightErrorUi::~CalypsoF19DogfightErrorUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF19DogfightErrorUi::topState() const { return _state; }
void CalypsoF19DogfightErrorUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF19DogfightErrorGen::layoutForDesign(wide?1280:740, wide?720:360);
    if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Interception warning layout is unavailable");
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF19DogfightErrorGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleWidget = _state->_txtCraft;
    m.titleText = _state->_txtCraft ? _state->_txtCraft->getText() : std::string();
    m.messageWidget = _state->_txtMessage;
    m.messageText = _state->_txtMessage ? _state->_txtMessage->getText() : std::string();
    m.protocolText = _state->tr("STR_INTERCEPT");
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.uiScale = uiScale;
    const auto &metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
    m.projectionScaleX = uiScale * metrics.scaleX;
    m.projectionScaleY = uiScale * metrics.scaleY;
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
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF19DogfightErrorGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF19DogfightErrorGen::kMotionScaleFrom;
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnIntercept; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF19DogfightErrorGen::kButtonCount;++i) if(std::string(CalypsoF19DogfightErrorGen::kButtons[i].id)=="intercept") { b.restFill=CalypsoF19DogfightErrorGen::kButtons[i].fill; b.restBorder=CalypsoF19DogfightErrorGen::kButtons[i].border; b.textColor=CalypsoF19DogfightErrorGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    { CalypsoSmallConfirmationButton b{}; b.widget = _state->_btnBase; b.text = b.widget ? b.widget->getText() : std::string(); b.rect = b.widget ? CalypsoLogicalRect{b.widget->getX(), b.widget->getY(), b.widget->getWidth(), b.widget->getHeight()} : proj(g->window); b.tone = CalypsoActionTone::Safe; for(int i=0;i<CalypsoF19DogfightErrorGen::kButtonCount;++i) if(std::string(CalypsoF19DogfightErrorGen::kButtons[i].id)=="tobase") { b.restFill=CalypsoF19DogfightErrorGen::kButtons[i].fill; b.restBorder=CalypsoF19DogfightErrorGen::kButtons[i].border; b.textColor=CalypsoF19DogfightErrorGen::kButtons[i].text; break; } m.buttons.push_back(b); }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF19DogfightErrorUi::configure(DogfightErrorState &s, bool allow)
{
	if (!allow || !s._game || !s._craft)
		CalypsoHdUiOverlay::instance().failHdRoute("Interception warning is unavailable");
	s._hdLayout = true;
	resize(s);
	s._hdAdapter = new CalypsoF19DogfightErrorUi(&s);
	CalypsoHdUiOverlay::instance().registerAdapter(s._hdAdapter);
	if (calypsoHarnessHostUp(calypsoHarnessSession())) calypsoHdHarnessDomShow();
}

bool CalypsoF19DogfightErrorUi::resize(DogfightErrorState &s)
{
	if (!s._hdLayout) return false;
	const auto viewport = calypsoHdOperationsEffectiveViewport();
	const bool wide = viewport.cssWidth >= 1000 && viewport.cssHeight >= 550;
	s._hdWideLayout = wide;
	namespace G = CalypsoF19DogfightErrorGen;
	const auto *g = G::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Interception warning layout is unavailable");
	auto place = [](Surface *widget, const auto &rect) {
		widget->setX(rect.x);
		widget->setY(rect.y);
		widget->setWidth(rect.w);
		widget->setHeight(rect.h);
	};
	place(s._window, g->window);
	place(s._txtCraft, g->title);
	place(s._txtMessage, g->message);
	for (const auto &button : G::kButtonRects[wide ? 0 : 1])
		place(std::string(button.id) == "intercept" ? s._btnIntercept : s._btnBase, button.rect);
	if (s._hdAdapter) s.recaptureUiScaling(g->designWidth, g->designHeight, 1.0f, false);
	else s.enableUiScaling(g->designWidth, g->designHeight, 1.0f, false);
	return true;
}
} }
#endif
