#ifdef __EMSCRIPTEN__
#include "CalypsoContactNotice.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoViewportRuntime.h"
#include "Generated/CalypsoF17UfoLost.generated.h"
#include "Generated/CalypsoF28TacticalAlert.generated.h"
#include "Generated/CalypsoF29AutomaticRecovery.generated.h"
#include "Generated/CalypsoF28TacticalNotice.generated.h"
#include "Generated/CalypsoF17AlienBase.generated.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"

namespace OpenXcom { namespace Calypso {
namespace {
template<class R> CalypsoLogicalRect rect(const R &r) { return {r.x,r.y,r.w,r.h}; }

template<class Layout, class Rect, class Button>
void readGeometry(CalypsoSmallConfirmationModel &m, const Layout &g, const Rect &action, const Button &style)
{
	m.designWidth = g.designWidth;
	m.designHeight = g.designHeight;
	m.window = rect(g.window);
	m.status = rect(g.status);
	m.warning = rect(g.warning);
	m.title = rect(g.title);
	m.message = rect(g.message);
	m.footer = rect(g.footer);
	m.messageDesignWidth = g.message.w;
	m.titleDesignHeight = g.title.h;
	CalypsoSmallConfirmationButton button;
	button.rect = rect(action);
	button.tone = std::string(style.tone) == "primary" ? CalypsoActionTone::Primary : CalypsoActionTone::Safe;
	button.restFill = style.fill;
	button.restBorder = style.border;
	button.textColor = style.text;
	m.buttons.push_back(button);
}

void place(Surface *widget, CalypsoLogicalRect r)
{
	widget->setX(r.x);
	widget->setY(r.y);
	widget->setWidth(r.w);
	widget->setHeight(r.h);
}
void lost(CalypsoSmallConfirmationModel &m, bool wide)
{
	namespace G = CalypsoF17UfoLostGen;
	const auto *g = G::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Contact notice layout is unavailable");
	readGeometry(m, *g, G::kButtonRects[wide ? 0 : 1][0].rect, G::kButtons[0]);
	m.familyId = G::kFamilyId;
	m.protocolText = G::kProtocol;
	m.visualScale = G::kPresentationScale;
	m.cutCornerPx = G::kCutCornerPx;
	m.protocolTextInsetPx = G::kProtocolTextInsetPx;
	m.panelFillTop = G::kPanelFillTop;
	m.panelFillBottom = G::kPanelFillBottom;
	m.frameColor = G::kFrame;
	m.protocolColor = G::kProtocolText;
	m.dividerColor = G::kDivider;
	m.footerDotColor = G::kFooterDot;
	m.warningColor = G::kWarning;
	m.motionDurationMs = G::kMotionDurationMs;
	m.motionScaleFrom = G::kMotionScaleFrom;
}
void discovered(CalypsoSmallConfirmationModel &m, bool wide)
{
	namespace G = CalypsoF17AlienBaseGen;
	const auto *g = G::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Contact notice layout is unavailable");
	readGeometry(m, *g, G::kButtonRects[wide ? 0 : 1][0].rect, G::kButtons[0]);
	m.familyId = G::kFamilyId;
	m.protocolText = G::kProtocol;
	m.visualScale = G::kPresentationScale;
	m.cutCornerPx = G::kCutCornerPx;
	m.protocolTextInsetPx = G::kProtocolTextInsetPx;
	m.panelFillTop = G::kPanelFillTop;
	m.panelFillBottom = G::kPanelFillBottom;
	m.frameColor = G::kFrame;
	m.protocolColor = G::kProtocolText;
	m.dividerColor = G::kDivider;
	m.footerDotColor = G::kFooterDot;
	m.warningColor = G::kWarning;
	m.motionDurationMs = G::kMotionDurationMs;
	m.motionScaleFrom = G::kMotionScaleFrom;
}
}
CalypsoContactNotice::CalypsoContactNotice(ContactNoticeType type, State *owner, const State *underlay,
	Mod *mod, Window *window, Text *message, TextButton *action, const std::string &title)
	: _type(type), _owner(owner), _underlay(underlay), _mod(mod), _window(window),
	  _message(message), _action(action), _title(title), _generation(calypsoHarnessSession().generation)
{
	if (!_mod || !_window || !_message || !_action)
		CalypsoHdUiOverlay::instance().failHdRoute("Contact notice is unavailable");
	layout();
	CalypsoHdUiOverlay::instance().registerAdapter(this);
	if (calypsoHarnessHostUp(calypsoHarnessSession())) calypsoHdHarnessDomShow();
}

CalypsoContactNotice::~CalypsoContactNotice()
{
	calypsoHdHarnessTeardownForTarget(_owner, _generation);
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

void CalypsoContactNotice::layout()
{
	auto &runtime = calypsoViewportRuntime();
	if (!runtime.hasLayout()) CalypsoHdUiOverlay::instance().failHdRoute("Contact notice viewport is unavailable");
	const auto &v = runtime.current();
	const bool wide = calypsoHarnessEffectiveLayout(calypsoHarnessSession(),
		{v.safeX, v.safeY, v.safeWidth, v.safeHeight}) == CalypsoLayoutClass::Wide;
	_design = {};
	_design.wide = wide;
	if (_type == ContactNoticeType::TrackingLost) lost(_design, wide);
	else if (_type == ContactNoticeType::TacticalNotice)
	{
		namespace G = CalypsoF28TacticalNoticeGen;
		const auto *layout = G::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		if (!layout) CalypsoHdUiOverlay::instance().failHdRoute("Tactical message layout is unavailable");
		readGeometry(_design, *layout, G::kButtonRects[wide ? 0 : 1][0].rect, G::kButtons[0]);
		_design.familyId = G::kFamilyId;
		_design.protocolText = G::kProtocol;
		_design.visualScale = G::kPresentationScale;
		_design.cutCornerPx = G::kCutCornerPx;
		_design.protocolTextInsetPx = G::kProtocolTextInsetPx;
		_design.panelFillTop = G::kPanelFillTop;
		_design.panelFillBottom = G::kPanelFillBottom;
		_design.frameColor = G::kFrame;
		_design.protocolColor = G::kProtocolText;
		_design.dividerColor = G::kDivider;
		_design.footerDotColor = G::kFooterDot;
		_design.warningColor = G::kWarning;
		_design.motionDurationMs = G::kMotionDurationMs;
		_design.motionScaleFrom = G::kMotionScaleFrom;
	}
	else if (_type == ContactNoticeType::TacticalAlert)
	{
		namespace G = CalypsoF28TacticalAlertGen;
		const auto *layout = G::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		if (!layout) CalypsoHdUiOverlay::instance().failHdRoute("Tactical message layout is unavailable");
		readGeometry(_design, *layout, G::kButtonRects[wide ? 0 : 1][0].rect, G::kButtons[0]);
		_design.familyId = G::kFamilyId;
		_design.protocolText = G::kProtocol;
		_design.visualScale = G::kPresentationScale;
		_design.cutCornerPx = G::kCutCornerPx;
		_design.protocolTextInsetPx = G::kProtocolTextInsetPx;
		_design.panelFillTop = G::kPanelFillTop;
		_design.panelFillBottom = G::kPanelFillBottom;
		_design.frameColor = G::kFrame;
		_design.protocolColor = G::kProtocolText;
		_design.dividerColor = G::kDivider;
		_design.footerDotColor = G::kFooterDot;
		_design.warningColor = G::kWarning;
		_design.motionDurationMs = G::kMotionDurationMs;
		_design.motionScaleFrom = G::kMotionScaleFrom;
	}
	else if (_type == ContactNoticeType::AutomaticRecovery)
	{
		namespace G = CalypsoF29AutomaticRecoveryGen;
		const auto *layout = G::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		if (!layout) CalypsoHdUiOverlay::instance().failHdRoute("Tactical message layout is unavailable");
		readGeometry(_design, *layout, G::kButtonRects[wide ? 0 : 1][0].rect, G::kButtons[0]);
		_design.familyId = G::kFamilyId;
		_design.protocolText = G::kProtocol;
		_design.visualScale = G::kPresentationScale;
		_design.cutCornerPx = G::kCutCornerPx;
		_design.protocolTextInsetPx = G::kProtocolTextInsetPx;
		_design.panelFillTop = G::kPanelFillTop;
		_design.panelFillBottom = G::kPanelFillBottom;
		_design.frameColor = G::kFrame;
		_design.protocolColor = G::kProtocolText;
		_design.dividerColor = G::kDivider;
		_design.footerDotColor = G::kFooterDot;
		_design.warningColor = G::kWarning;
		_design.motionDurationMs = G::kMotionDurationMs;
		_design.motionScaleFrom = G::kMotionScaleFrom;
	}
	else discovered(_design, wide);
	place(_window, _design.window);
	place(_message, _design.message);
	auto hit = _design.buttons.front().rect;
	const int height = std::max(hit.h, CALYPSO_MIN_TOUCH_TARGET);
	hit.y -= (height - hit.h) / 2;
	hit.h = height;
	place(_action, hit);
}

void CalypsoContactNotice::collect(CalypsoHdFrameBuilder &builder) const
{
	auto m = _design;
	const auto scale = double(_window->getWidth()) / m.window.w;
	auto project = [&](CalypsoLogicalRect &r) {
		r = {_window->getX() + int(std::lround((r.x - _design.window.x) * scale)),
			_window->getY() + int(std::lround((r.y - _design.window.y) * scale)),
			int(std::lround(r.w * scale)), int(std::lround(r.h * scale))};
	};
	for (auto *r : {&m.window, &m.status, &m.warning, &m.title, &m.message, &m.footer}) project(*r);
	auto &button = m.buttons.front();
	project(button.rect);
	button.widget = _action;
	button.text = _action->getText();
	m.instance = _owner;
	m.mod = _mod;
	m.windowWidget = _window;
	m.messageWidget = _message;
	m.titleText = _title;
	m.messageText = _message->getText();
	m.uiScale = scale;
	const auto &metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
	m.projectionScaleX = scale * metrics.scaleX;
	m.projectionScaleY = scale * metrics.scaleY;
	calypsoCollectSmallConfirmation(builder, m, _motion);
}
} }
#endif
