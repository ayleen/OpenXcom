#ifdef __EMSCRIPTEN__
#include "CalypsoF17MissionDetectedUi.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Geoscape/MissionDetectedState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF17MissionDetected.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
void CalypsoF17MissionDetectedUi::applyLayout(MissionDetectedState& state)
{
	const bool wide = state._hdWideLayout;
	const auto* layout = CalypsoF17MissionDetectedGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!layout)
		CalypsoHdUiOverlay::instance().failHdRoute(
			"F17 mission contact layout is unavailable");
	auto applyRect = [](auto* widget, const auto& rect)
	{
		if (!widget) return;
		widget->setX(rect.x);
		widget->setY(rect.y);
		widget->setWidth(rect.w);
		widget->setHeight(rect.h);
	};
	applyRect(state._window, layout->window);
	const int layoutIndex = wide ? 0 : 1;
	applyRect(state._btnIntercept,
		CalypsoF17MissionDetectedGen::kButtonRects[layoutIndex][0].rect);
	applyRect(state._btnCenter,
		CalypsoF17MissionDetectedGen::kButtonRects[layoutIndex][1].rect);
	applyRect(state._btnCancel,
		CalypsoF17MissionDetectedGen::kButtonRects[layoutIndex][2].rect);
}
CalypsoF17MissionDetectedUi::~CalypsoF17MissionDetectedUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF17MissionDetectedUi::topState() const { return _state; }
void CalypsoF17MissionDetectedUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    const bool wide = _state->_hdWideLayout;
    const int layoutIndex = wide ? 0 : 1;
    const auto* g = CalypsoF17MissionDetectedGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](const auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF17MissionDetectedGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    m.titleWidget = _state->_txtTitle;
    m.titleText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
    m.messageWidget = _state->_txtCity;
    m.messageText = _state->_txtCity ? _state->_txtCity->getText() : std::string();
    m.protocolText = CalypsoF17MissionDetectedGen::kProtocol;
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF17MissionDetectedGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF17MissionDetectedGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF17MissionDetectedGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF17MissionDetectedGen::kPanelFillBottom;
    m.frameColor = CalypsoF17MissionDetectedGen::kFrame;
    m.protocolColor = CalypsoF17MissionDetectedGen::kProtocolText;
    m.dividerColor = CalypsoF17MissionDetectedGen::kDivider;
    m.footerDotColor = CalypsoF17MissionDetectedGen::kFooterDot;
    m.warningColor = CalypsoF17MissionDetectedGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF17MissionDetectedGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF17MissionDetectedGen::kMotionScaleFrom;
    auto addButton = [&](TextButton* widget, int index, const std::string& label)
    {
        CalypsoSmallConfirmationButton button{};
        button.widget = widget;
        button.text = label;
        button.rect = proj(CalypsoF17MissionDetectedGen::kButtonRects[layoutIndex][index].rect);
        button.tone = CalypsoActionTone::Safe;
        button.restFill = CalypsoF17MissionDetectedGen::kButtons[index].fill;
        button.restBorder = CalypsoF17MissionDetectedGen::kButtons[index].border;
        button.textColor = CalypsoF17MissionDetectedGen::kButtons[index].text;
        m.buttons.push_back(button);
    };
    addButton(_state->_btnIntercept, 0,
        _state->_btnIntercept ? _state->_btnIntercept->getText() : std::string());
    addButton(_state->_btnCenter, 1, _state->tr("STR_CENTER"));
    addButton(_state->_btnCancel, 2,
        _state->_btnCancel ? _state->_btnCancel->getText() : std::string());
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF17MissionDetectedUi::configure(MissionDetectedState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F17")) { s._hdLayout=false; return; }
    s._hdLayout = true;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    applyLayout(s);
    auto* a = new CalypsoF17MissionDetectedUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF17MissionDetectedUi::resize(MissionDetectedState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    applyLayout(s);
    return true;
}
} }
#endif
