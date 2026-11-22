#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoViewportRuntime.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"

namespace OpenXcom { namespace Calypso {
inline bool calypsoFleetAlertWide()
{
	auto &runtime = calypsoViewportRuntime();
	if (!runtime.hasLayout()) CalypsoHdUiOverlay::instance().failHdRoute("Fleet alert viewport unavailable");
	const auto &v = runtime.current();
	return calypsoHarnessEffectiveLayout(calypsoHarnessSession(), {v.safeX,v.safeY,v.safeWidth,v.safeHeight}) == CalypsoLayoutClass::Wide;
}

template<class Rect>
void calypsoPlaceFleetWidget(Surface &widget, const Rect &r)
{
	widget.setX(r.x);
	widget.setY(r.y);
	widget.setWidth(r.w);
	widget.setHeight(r.h);
}

template<class Layout, class Rects>
void calypsoPlaceFleetAlert(const Layout &layout, const Rects &buttons,
	Window &window, Text &message, Text *title, TextButton &ack, TextButton &action)
{
	calypsoPlaceFleetWidget(window, layout.window);
	calypsoPlaceFleetWidget(message, layout.message);
	if (title) calypsoPlaceFleetWidget(*title, layout.title);
	calypsoPlaceFleetWidget(ack, buttons[0].rect);
	calypsoPlaceFleetWidget(action, buttons[1].rect);
}

template<class Layout, class Rects, class Styles>
void calypsoCollectFleetAlert(CalypsoHdFrameBuilder &builder, CalypsoSmallConfirmationModel &m,
	CalypsoSmallConfirmationMotion &motion, const Layout &g, const Rects &buttons,
	const Styles &styles, Window &window, Text &message, Text *title, TextButton &ack, TextButton &action)
{
	const double scale = double(window.getWidth()) / g.window.w;
	auto project = [&](const auto &r) -> CalypsoLogicalRect {
		return {window.getX() + int(std::lround((r.x - g.window.x) * scale)),
			window.getY() + int(std::lround((r.y - g.window.y) * scale)),
			int(std::lround(r.w * scale)), int(std::lround(r.h * scale))};
	};
	m.designWidth = g.designWidth;
	m.designHeight = g.designHeight;
	m.window = project(g.window);
	m.status = project(g.status);
	m.warning = project(g.warning);
	m.title = project(g.title);
	m.message = project(g.message);
	m.footer = project(g.footer);
	m.windowWidget = &window;
	m.titleWidget = title;
	m.messageWidget = &message;
	m.messageText = message.getText();
	if (title) m.titleText = title->getText();
	m.messageDesignWidth = g.message.w;
	m.titleDesignHeight = g.title.h;
	m.uiScale = scale;
	const auto &metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
	m.projectionScaleX = scale * metrics.scaleX;
	m.projectionScaleY = scale * metrics.scaleY;
	TextButton *owners[] = {&ack, &action};
	for (int i = 0; i < 2; ++i)
	{
		if (!owners[i]->getVisible()) continue;
		CalypsoSmallConfirmationButton b;
		b.widget = owners[i];
		b.text = owners[i]->getText();
		b.rect = project(buttons[i].rect);
		b.tone = std::string(styles[i].tone) == "primary" ? CalypsoActionTone::Primary : CalypsoActionTone::Safe;
		b.restFill = styles[i].fill;
		b.restBorder = styles[i].border;
		b.textColor = styles[i].text;
		m.buttons.push_back(b);
	}
	calypsoCollectSmallConfirmation(builder, m, motion);
}
} }
#endif
