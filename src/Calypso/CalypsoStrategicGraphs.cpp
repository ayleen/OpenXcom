#ifdef __EMSCRIPTEN__
#include "CalypsoStrategicGraphs.h"
#include "CalypsoHdImageSource.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdUiOverlay.h"
#include "Generated/CalypsoF23StrategicGraphs.generated.h"
#include "../Geoscape/GraphsState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/GameTime.h"

namespace OpenXcom
{
namespace Calypso
{
namespace Gen = CalypsoF23StrategicGraphsGen;
namespace
{
constexpr const char *tabs[] = {"STR_CAL_GRAPH_ALIEN_SEAS", "STR_CAL_GRAPH_ALIEN_ZONES",
	"STR_CAL_GRAPH_XCOM_SEAS", "STR_CAL_GRAPH_XCOM_ZONES", "STR_INCOME", "STR_FINANCE"};
CalypsoLogicalRect graphRect(const CalypsoLogicalRect &window, const Gen::CalypsoF23StrategicGraphsGenLayout &g,
	const Gen::CalypsoF23StrategicGraphsGenRect &rect)
{
	const auto x = [&](int coordinate) { return window.x + int(std::lround((coordinate - g.window.x) * double(window.w) / g.window.w)); };
	const auto y = [&](int coordinate) { return window.y + int(std::lround((coordinate - g.window.y) * double(window.h) / g.window.h)); };
	return {x(rect.x), y(rect.y), std::max(1, x(rect.x + rect.w) - x(rect.x)),
		std::max(1, y(rect.y + rect.h) - y(rect.y))};
}

constexpr const char *patterns[] = {"---", "- -", "..."};
constexpr unsigned colors[] = {0x6deab5, 0xffbc74, 0x79b9ff, 0xf994af, 0xd4b1ff, 0xe2dc74,
	0x79e0ed, 0xa7bcff, 0xff9463, 0xc6eb7c, 0xee99df, 0xf4eedd};
}

CalypsoStrategicGraphs::CalypsoStrategicGraphs(GraphsState &state) : _state(state)
{
	for (auto *surface : state._surfaces) surface->setVisible(false);
	auto bind = [&](auto &buttons) {
		for (auto &button : buttons)
		{
			button = new TextButton(1, 1, 0, 0);
			state.add(button, "button", "graphs");
			button->onMouseClick((ActionHandler)&GraphsState::hdClick);
		}
	};
	bind(_tabs);
	bind(_toolbar);
	bind(_actions);
	bind(_rows);
	_actions.back()->onKeyboardPress((ActionHandler)&GraphsState::btnGeoscapeClick, Options::keyCancel);
	_actions.back()->onKeyboardPress((ActionHandler)&GraphsState::btnGeoscapeClick, Options::keyGeoGraphs);
	_actions.back()->onKeyboardPress((ActionHandler)&GraphsState::btnZoomInClick, Options::keyGraphsZoomIn);
	_actions.back()->onKeyboardPress((ActionHandler)&GraphsState::btnZoomOutClick, Options::keyGraphsZoomOut);
	layout();
	CalypsoHdUiOverlay::instance().registerAdapter(this);
}

CalypsoStrategicGraphs::~CalypsoStrategicGraphs()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}
const void *CalypsoStrategicGraphs::topState() const { return &_state; }
void CalypsoStrategicGraphs::collectLogicalSuppression(CalypsoHdLogicalSuppression &suppression) const
{
	for (auto *surface : _state._surfaces) suppression.add(surface);
}

void CalypsoStrategicGraphs::layout()
{
	const auto viewport = calypsoHdOperationsEffectiveViewport();
	const int w = viewport.cssWidth, h = viewport.cssHeight;
	_layout = w >= 1000 && h >= 550 ? 0 : 1;
	const auto &g = Gen::kLayouts[_layout];
	const double scale = std::min(double(w) / g.designWidth, double(h) / g.designHeight);
	const int width = std::max(1, int(std::lround(g.window.w * scale)));
	const int height = std::max(1, int(std::lround(g.window.h * scale)));
	// Native input and tabbed paint both use the state's logical resolution.
	const int logicalWidth = std::max(1, Options::baseXResolution / (viewport.sideBySide ? 2 : 1));
	const int logicalHeight = std::max(1, Options::baseYResolution);
	_window.w = std::max(1, int(std::lround(double(width) * logicalWidth / w)));
	_window.h = std::max(1, int(std::lround(double(height) * logicalHeight / h)));
	_window.x = (logicalWidth - _window.w) / 2;
	_window.y = (logicalHeight - _window.h) / 2;
	const size_t count = _state._series.size();
	const size_t pageSize = g.visibleRows;
	_page = std::min(_page, count ? (count - 1) / pageSize : size_t(0));
	auto position = [&](auto &buttons, const auto &rects, size_t visible) {
		for (size_t i = 0; i < buttons.size(); ++i)
		{
			auto *button = buttons[i];
			button->setVisible(i < visible);
			if (i >= visible) continue;
			const auto r = graphRect(_window, g, rects[i].rect);
			button->setX(r.x);
			button->setY(r.y);
			button->setWidth(r.w);
			button->setHeight(r.h);
		}
	};
	position(_tabs, Gen::kTabRects[_layout], _tabs.size());
	position(_toolbar, Gen::kToolbarRects[_layout], _toolbar.size());
	position(_actions, Gen::kActionRects[_layout], _actions.size());
	position(_rows, Gen::kRowSlots[_layout], _legend ? std::min(pageSize, count - _page * pageSize) : 0);
}

void CalypsoStrategicGraphs::click(Action *action)
{
	auto *sender = action->getSender();
	for (size_t i = 0; i < _tabs.size(); ++i)
		if (sender == _tabs[i])
		{
			_mode = i;
			_page = 0;
			_state._finance = i == 5;
			_state._income = i == 4;
			_state._country = i == 1 || i == 3 || i == 4;
			_state._alien = i < 2;
			_state.drawLines();
			layout();
			return;
		}
	if (sender == _toolbar[0]) _legend = !_legend;
	else if (sender == _toolbar[1] || sender == _toolbar[2])
	{
		for (size_t i = 0; i < _state._series.size(); ++i)
			_state.hdSetVisible(i, sender == _toolbar[1]);
		_state.drawLines();
	}
	else if (sender == _actions[2]) { _state.btnGeoscapeClick(action); return; }
	else if (sender == _actions[0] || sender == _actions[1])
	{
		if (_legend)
		{
			if (sender == _actions[1]) ++_page;
			else if (_page) --_page;
		}
		else if (sender == _actions[1]) _state.btnZoomInClick(action);
		else _state.btnZoomOutClick(action);
	}
	else for (size_t i = 0; i < _rows.size(); ++i)
		if (_legend && sender == _rows[i])
		{
			const size_t index = _page * Gen::kLayouts[_layout].visibleRows + i;
			if (index < _state._series.size())
			{
				_state.hdSetVisible(index, !_state._series[index].visible);
				_state.drawLines();
			}
			break;
		}
	layout();
}

void CalypsoStrategicGraphs::updatePlot(int width, int height) const
{
	const int w = std::clamp(width, 2, kCalypsoHdImageMaxExtent);
	const int h = std::clamp(height, 2, kCalypsoHdImageMaxExtent);
	if (_plot.pixels && _revision == _state._hdRevision && _plot.pixels->w == w && _plot.pixels->h == h) return;
	auto image = std::make_shared<CalypsoHdImageRgba>();
	image->w = w;
	image->h = h;
	image->px.resize(size_t(w) * h * 4);
	auto pixel = [&](int x, int y, unsigned rgb) {
		auto *p = image->px.data() + (size_t(y) * w + x) * 4;
		p[0] = rgb >> 16; p[1] = rgb >> 8; p[2] = rgb; p[3] = 255;
	};
	auto xAt = [&](int m) { return (11 - m) * (w - 1) / 11; };
	for (int m = 0; m < 12; ++m)
		for (int y = 0; y < h; ++y) pixel(xAt(m), y, 0x29444d);
	for (int tick = 0; tick <= 3; ++tick)
		for (int x = 0; x < w; ++x) pixel(x, tick * (h - 1) / 3, 0x416069);
	const int stroke = std::max(2, w / 480);
	for (size_t i = 0; i < _state._series.size(); ++i)
	{
		const auto &series = _state._series[i];
		if (!series.visible) continue;
		auto yAt = [&](int m) { return int(std::lround((1 - _state._axis.fraction(series.values[m])) * (h - 1))); };
		int distance = 0;
		for (int m = 1; m < 12; ++m)
		{
			const int x0 = xAt(m), x1 = xAt(m - 1), y0 = yAt(m), y1 = yAt(m - 1);
			const int steps = std::max({1, x1 - x0, std::abs(y1 - y0)});
			for (int n = 0; n <= steps; ++n)
			{
				const int x = x0 + (x1 - x0) * n / steps, y = y0 + (y1 - y0) * n / steps;
				const int period = (i % 3 == 1 ? 12 : 5) * stroke;
				if (i % 3 && (distance + n) % period >= period / 2) continue;
				for (int dx = 0; dx < stroke && x + dx < w; ++dx)
					for (int dy = 0; dy < stroke && y + dy < h; ++dy)
						pixel(x + dx, y + dy, colors[i % std::size(colors)]);
			}
			distance += steps;
		}
	}
	static unsigned generation = 0;
	_plot.source = "calypso/campaign-graph";
	_plot.generation = ++generation;
	_plot.pixels = std::move(image);
	_revision = _state._hdRevision;
}

void CalypsoStrategicGraphs::collect(CalypsoHdFrameBuilder &builder) const
{
	const auto &g = Gen::kLayouts[_layout];
	const auto &metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
	const double scale = double(_window.w) / g.window.w;
	auto project = [&](const auto &rect) { return graphRect(_window, g, rect); };
	const auto tr = [&](const char *id) -> std::string { return _state.tr(id); };
	CalypsoTabbedModel model;
	model.instance = &_state;
	model.familyId = Gen::kFamilyId;
	model.mod = _state._game->getMod();
	model.wide = _layout == 0;
	model.designWidth = g.designWidth;
	model.designHeight = g.designHeight;
	model.window = _window;
	model.title = project(g.title);
	model.summaryBar = project(g.summaryBar);
	model.tabBar = project(g.tabBar);
	model.toolbarBar = project(g.toolbarBar);
	model.collectionViewport = project(g.collectionViewport);
	model.footer = project(g.footer);
	model.windowWidget = _state._bg;
	model.titleWidget = _state._txtTitle;
	model.listWidget = _state._txtMonths;
	constexpr const char *titles[] = {"STR_UFO_ACTIVITY_IN_AREAS", "STR_UFO_ACTIVITY_IN_COUNTRIES",
		"STR_XCOM_ACTIVITY_IN_AREAS", "STR_XCOM_ACTIVITY_IN_COUNTRIES", "STR_INCOME", "STR_FINANCE"};
	model.titleText = tr(titles[_mode]);
	model.visibleRows = g.visibleRows;
	model.rowHeight = g.rowHeight;
	model.cutCornerPx = Gen::kCutCornerPx;
	model.panelFillTop = Gen::kPanelFillTop;
	model.panelFillBottom = Gen::kPanelFillBottom;
	model.frameColor = Gen::kFrame;
	model.selectedTabColor = Gen::kSelectedTab;
	model.dividerColor = Gen::kDivider;
	model.textColor = Gen::kText;
	model.mutedTextColor = Gen::kMutedText;
	model.selectionColor = Gen::kSelectedTab;
	model.scrollTrackColor = Gen::kPanelFillBottom;
	model.uiScale = scale;
	model.projectionScaleX = scale * metrics.scaleX;
	model.projectionScaleY = double(_window.h) / g.window.h * metrics.scaleY;
	model.textRasterScale = std::max(1.0, model.projectionScaleY);
	model.titleDesignHeight = g.title.h;
	model.motionDurationMs = Gen::kMotionDurationMs;
	model.motionScaleFrom = Gen::kMotionScaleFrom;

	const auto *date = _state._game->getSavedGame()->getTime();
	const int year = date->getYear();
	const auto month = [&](int slot) { return _state._txtMonths->getCellText(0, slot); };
	const auto start = month(0) + " " + std::to_string(year - (date->getMonth() != 12));
	const auto end = month(11) + " " + std::to_string(year);
	model.summary.push_back({start + " – " + end, project(Gen::kSummaryRects[_layout][0].rect)});
	auto units = _mode >= 4 ? tr("STR_FINANCE_THOUSANDS") : std::string();
	model.summary.push_back({units + "  " + std::to_string(int(std::lround(10000.0 / _state._zoom))) + "%",
		project(Gen::kSummaryRects[_layout][1].rect)});
	for (size_t i = 0; i < _tabs.size(); ++i)
		model.tabs.push_back({Gen::kTabIds[i], tr(tabs[i]), int(i) == _mode, true, _tabs[i], project(Gen::kTabRects[_layout][i].rect)});

	auto addButton = [&](auto &target, TextButton *button, const auto &spec, const auto &rect, const char *id) {
		target.push_back({button, nullptr, tr(id), project(rect), calypsoTabbedToneForContract(spec.tone),
			spec.fill, spec.border, spec.text});
	};
	const char *toolbar[] = {_legend ? "STR_CAL_GRAPH_CHART" : "STR_CAL_GRAPH_SERIES", "STR_CAL_GRAPH_ALL", "STR_CAL_GRAPH_NONE"};
	const char *actions[] = {_legend ? "STR_CAL_GRAPH_PREVIOUS" : "STR_CAL_GRAPH_ZOOM_OUT",
		_legend ? "STR_CAL_GRAPH_NEXT" : "STR_CAL_GRAPH_ZOOM_IN", "STR_OK"};
	for (size_t i = 0; i < _actions.size(); ++i)
	{
		addButton(model.toolbar, _toolbar[i], Gen::kToolbar[i], Gen::kToolbarRects[_layout][i].rect, toolbar[i]);
		addButton(model.actions, _actions[i], Gen::kActions[i], Gen::kActionRects[_layout][i].rect, actions[i]);
	}
	if (_legend)
	{
		model.columns.push_back({"series", tr("STR_CAL_GRAPH_SERIES"), project(Gen::kColumnHeaders[_layout][0].rect)});
		model.columns.push_back({"value", end, project(Gen::kColumnHeaders[_layout][1].rect)});
		for (int slot = 0; slot < g.visibleRows; ++slot)
		{
			const size_t i = _page * g.visibleRows + slot;
			if (i >= _state._series.size()) break;
			model.rowSlots.push_back(project(Gen::kRowSlots[_layout][slot].rect));
			const auto &series = _state._series[i];
			const std::string marker = series.visible ? "[+] " : "[ ] ";
			model.rows.push_back({{}, {marker + patterns[i % std::size(patterns)] + " " + _state.hdName(i),
				std::to_string(std::llround(series.values[0]))}, true, (colors[i % std::size(colors)] << 8) | 255u});
		}
	}
	else
	{
		auto rect = g.collectionViewport;
		const int axisWidth = _layout == 0 ? 62 : 52, tickHeight = 20;
		rect.x += axisWidth; rect.y += 8;
		rect.w -= axisWidth + 16; rect.h -= tickHeight + 12;
		model.plotRect = project(rect);
		updatePlot(std::lround(model.plotRect.w * metrics.scaleX), std::lround(model.plotRect.h * metrics.scaleY));
		model.plotImage = _plot;
		for (int i = 0; i < 4; ++i)
		{
			auto label = rect;
			label.x -= axisWidth - 3; label.w = axisWidth - 4; label.h = tickHeight;
			label.y += (rect.h - tickHeight) * i / 3;
			model.plotLabels.push_back({std::to_string(std::llround(_state._axis.high + (_state._axis.low - _state._axis.high) * i / 3)), project(label)});
		}
		for (int m = 0; m < 12; ++m)
		{
			auto label = rect;
			label.w = rect.w / 12; label.h = tickHeight;
			label.x += (rect.w - label.w) * m / 11; label.y += rect.h;
			model.plotLabels.push_back({month(m), project(label)});
		}
		if (std::none_of(_state._series.begin(), _state._series.end(), [](const auto &line){return line.visible;}))
		{
			auto hint = rect;
			hint.x += 12; hint.y += rect.h / 2 - tickHeight; hint.h = 2 * tickHeight;
			model.plotLabels.push_back({tr("STR_CAL_GRAPH_EMPTY"), project(hint)});
		}
	}
	calypsoCollectTabbedManagement(builder, model, _motion);
}
}
}
#endif
