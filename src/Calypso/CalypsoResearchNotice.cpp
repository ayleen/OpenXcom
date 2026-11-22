#ifdef __EMSCRIPTEN__
#include "CalypsoResearchNotice.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoViewportRuntime.h"
#include "Generated/CalypsoF24ResearchRequired.generated.h"
#include "Generated/CalypsoF24ResearchComplete.generated.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"

namespace OpenXcom { namespace Calypso {
namespace {
template<class R> CalypsoLogicalRect rect(const R &r) { return {r.x,r.y,r.w,r.h}; }

template<class Layout, class Rects, class Styles>
void readGeometry(CalypsoSmallConfirmationModel &m, const Layout &g, const Rects &rects,
	const Styles &styles, int count)
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
	for (int i = 0; i < count; ++i)
	{
		CalypsoSmallConfirmationButton b;
		b.rect = rect(rects[i].rect);
		b.tone = std::string(styles[i].tone) == "primary" ? CalypsoActionTone::Primary : CalypsoActionTone::Safe;
		b.restFill = styles[i].fill;
		b.restBorder = styles[i].border;
		b.textColor = styles[i].text;
		m.buttons.push_back(b);
	}
}

void place(Surface *widget, CalypsoLogicalRect r)
{
	widget->setX(r.x);
	widget->setY(r.y);
	widget->setWidth(r.w);
	widget->setHeight(r.h);
}
void required(CalypsoSmallConfirmationModel &m, bool wide)
{
	namespace G = CalypsoF24ResearchRequiredGen;
	const auto *g = G::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Research notice layout is unavailable");
	readGeometry(m, *g, G::kButtonRects[wide ? 0 : 1], G::kButtons, G::kButtonCount);
	m.familyId = G::kFamilyId;
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
void complete(CalypsoSmallConfirmationModel &m, bool wide)
{
	namespace G = CalypsoF24ResearchCompleteGen;
	const auto *g = G::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Research notice layout is unavailable");
	readGeometry(m, *g, G::kButtonRects[wide ? 0 : 1], G::kButtons, G::kButtonCount);
	m.familyId = G::kFamilyId;
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
CalypsoResearchNotice::CalypsoResearchNotice(State *owner, const State *underlay, Mod *mod,
	Window *window, Text *message, std::string title, std::string context, TextButton *ok, TextButton *report)
	: _owner(owner), _underlay(underlay), _mod(mod), _window(window), _message(message),
	  _title(std::move(title)), _context(std::move(context)), _ok(ok), _report(report),
	  _generation(calypsoHarnessSession().generation)
{
	if (!_mod || !_window || !_message || !_ok)
		CalypsoHdUiOverlay::instance().failHdRoute("Research notice is unavailable");
	layout();
	CalypsoHdUiOverlay::instance().registerAdapter(this);
	if (calypsoHarnessHostUp(calypsoHarnessSession())) calypsoHdHarnessDomShow();
}

CalypsoResearchNotice::~CalypsoResearchNotice()
{
	calypsoHdHarnessTeardownForTarget(_owner, _generation);
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

void CalypsoResearchNotice::layout()
{
	auto &runtime = calypsoViewportRuntime();
	if (!runtime.hasLayout()) CalypsoHdUiOverlay::instance().failHdRoute("Research notice viewport is unavailable");
	const auto &v = runtime.current();
	const bool wide = calypsoHarnessEffectiveLayout(calypsoHarnessSession(),
		{v.safeX, v.safeY, v.safeWidth, v.safeHeight}) == CalypsoLayoutClass::Wide;
	_design = {};
	_design.wide = wide;
	if (_report) complete(_design, wide);
	else required(_design, wide);
	place(_window, _design.window);
	place(_message, _design.message);
	for (std::size_t i = 0; i < _design.buttons.size(); ++i)
	{
		auto &button = _design.buttons[i];
		button.widget = i == 0 ? _ok : _report;
		button.text = button.widget->getText();
		auto hit = button.rect;
		const int height = std::max(hit.h, CALYPSO_MIN_TOUCH_TARGET);
		hit.y -= (height - hit.h) / 2;
		hit.h = height;
		place(button.widget, hit);
	}
}

void CalypsoResearchNotice::collect(CalypsoHdFrameBuilder &builder) const
{
	auto m = _design;
	const auto scale = double(_window->getWidth()) / m.window.w;
	auto project = [&](CalypsoLogicalRect &r) {
		r = {_window->getX() + int(std::lround((r.x - _design.window.x) * scale)),
			_window->getY() + int(std::lround((r.y - _design.window.y) * scale)),
			int(std::lround(r.w * scale)), int(std::lround(r.h * scale))};
	};
	for (auto *r : {&m.window, &m.status, &m.warning, &m.title, &m.message, &m.footer}) project(*r);
	for (auto &button : m.buttons) project(button.rect);
	m.protocolText = _context;
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
