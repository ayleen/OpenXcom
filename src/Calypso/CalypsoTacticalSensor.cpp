#ifdef __EMSCRIPTEN__
#include "CalypsoTacticalSensor.h"
#include "../Battlescape/Camera.h"
#include "../Battlescape/MiniMapState.h"
#include "../Battlescape/MiniMapView.h"
#include "../Battlescape/ScannerState.h"
#include "../Battlescape/ScannerView.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/State.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/SavedBattleGame.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdImageSource.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdUiOverlay.h"
#include "Generated/CalypsoF25Minimap.generated.h"
#include "Generated/CalypsoF25Scanner.generated.h"

namespace OpenXcom
{
namespace Calypso
{
namespace MapForm = CalypsoF25MinimapGen;
namespace ScanForm = CalypsoF25ScannerGen;
namespace
{
void place(Surface& surface, CalypsoLogicalRect r)
{
	surface.setX(r.x);
	surface.setY(r.y);
	surface.setWidth(r.w);
	surface.setHeight(r.h);
}
const char* label(const std::string& id)
{
	if (id == "left")
		return "STR_SCROLL_LEFT";
	if (id == "right")
		return "STR_SCROLL_RIGHT";
	if (id == "up")
		return "STR_SCROLL_UP";
	if (id == "down")
		return "STR_SCROLL_DOWN";
	if (id == "lower")
		return "STR_VIEW_LEVEL_BELOW";
	if (id == "higher")
		return "STR_VIEW_LEVEL_ABOVE";
	if (id == "center")
		return "STR_CAL_SENSOR_RECENTER";
	if (id == "readout")
		return "STR_CAL_SENSOR_READOUT";
	if (id == "legend")
		return "STR_CAL_SENSOR_LEGEND";
	return "STR_CAL_SENSOR_CLOSE";
}
}
CalypsoTacticalSensor::CalypsoTacticalSensor(State& owner, MiniMapView* map, ScannerView* scanner,
											 Camera* camera, SavedBattleGame& battle, ActionHandler click, const std::string& title)
	: _owner(owner), _map(map), _scanner(scanner), _camera(camera), _battle(battle), _title(title)
{
	_session = calypsoHarnessSession().generation;
	auto bind = [&](const char* id, SDLKey key)
	{
		auto* button = new TextButton(1, 1, 0, 0);
		_owner.add(button);
		button->setText(_owner.tr(label(id)));
		button->onMouseClick(click);
		if (key != SDLK_UNKNOWN)
			button->onKeyboardPress(click, key);
		_buttons.emplace(id, button);
	};
	bind("done", Options::keyCancel);
	if (_map)
	{
		_buttons.at("done")->onKeyboardPress(click, Options::keyBattleMap);
		bind("left", Options::keyBattleLeft);
		bind("right", Options::keyBattleRight);
		bind("up", Options::keyBattleUp);
		bind("down", Options::keyBattleDown);
		bind("lower", Options::keyBattleLevelDown);
		bind("higher", Options::keyBattleLevelUp);
		bind("center", Options::keyBattleCenterUnit);
	}
	else
	{
		bind("readout", SDLK_1);
		bind("legend", SDLK_2);
	}
	_model.titleWidget = new Text(1, 1, 0, 0);
	_owner.add(_model.titleWidget);
	_model.listWidget = new TextList(1, 1, 0, 0);
	_owner.add(_model.listWidget);
	_model.listWidget->setVisible(false);
	layout();
	refresh();
	CalypsoHdUiOverlay::instance().registerAdapter(this);
	if (calypsoHarnessHostUp(calypsoHarnessSession()))
		calypsoHdHarnessDomShow();
}
CalypsoTacticalSensor::~CalypsoTacticalSensor()
{
	calypsoHdHarnessTeardownForTarget(&_owner, _session);
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}
void CalypsoTacticalSensor::layout()
{
	const auto vp = calypsoHdOperationsEffectiveViewport();
	const int layout = vp.cssWidth >= 1000 && vp.cssHeight >= 550 ? 0 : 1;
	auto& m = _model;
	auto configure = [&](const auto& g, const auto& tabs, int tabCount, const auto& tools, int toolCount,
						 const auto& actions, int count, const auto& toolRects, const auto& actionRects, const auto& summaryRect)
	{
		const double factor = std::min(double(vp.cssWidth) / g.designWidth, double(vp.cssHeight) / g.designHeight);
		const double sx = factor * vp.projectionLogicalWidth / vp.cssWidth;
		const double sy = factor * vp.projectionLogicalHeight / vp.cssHeight;
		const int x = (vp.projectionLogicalWidth - int(std::lround(g.designWidth * sx))) / 2;
		const int y = (vp.projectionLogicalHeight - int(std::lround(g.designHeight * sy))) / 2;
		auto project = [&](const auto& r)
		{ return CalypsoLogicalRect{x + int(std::lround(r.x * sx)),
									y + int(std::lround(r.y * sy)), std::max(1, int(std::lround(r.w * sx))), std::max(1, int(std::lround(r.h * sy)))}; };
		m.wide = layout == 0;
		m.designWidth = g.designWidth;
		m.designHeight = g.designHeight;
		m.window = project(g.window);
		m.title = project(g.title);
		m.summaryBar = project(g.summaryBar);
		m.tabBar = project(g.tabBar);
		m.toolbarBar = project(g.toolbarBar);
		m.footer = project(g.footer);
		m.collectionViewport = project(g.collectionViewport);
		m.titleDesignHeight = g.title.h;
		m.summary = {{"", project(summaryRect)}};
		m.uiScale = sx;
		m.tabs.clear();
		m.toolbar.clear();
		m.actions.clear();
		for (int i = 0; i < tabCount; ++i)
		{
			auto* widget = _buttons.at(tabs[i].id);
			const auto rect = project(tabs[i].rect);
			place(*widget, rect);
			m.tabs.push_back({tabs[i].id, widget->getText(), !_map && (std::string(tabs[i].id) == "legend") == _legend, true, widget, rect});
		}
		auto action = [&](auto& target, const auto& spec, const auto& r)
		{
			auto* widget = _buttons.at(spec.id);
			const auto rect = project(r);
			place(*widget, rect);
			target.push_back({widget, nullptr, widget->getText(), rect, calypsoTabbedToneForContract(spec.tone), spec.fill, spec.border, spec.text});
		};
		for (int i = 0; i < toolCount; ++i)
			action(m.toolbar, tools[i], toolRects[i].rect);
		for (int i = 0; i < count; ++i)
			action(m.actions, actions[i], actionRects[i].rect);
		auto area = m.collectionViewport;
		if (!_map)
		{
			area.h += area.y - m.toolbarBar.y;
			area.y = m.toolbarBar.y;
			m.collectionViewport = area;
		}
		const double ratio = _map ? 221.0 / 148.0 : 1.0;
		const double logicalRatio = ratio * sx / sy;
		m.plotRect.w = std::min(area.w, std::max(1, int(std::lround(area.h * logicalRatio))));
		m.plotRect.h = std::min(area.h, std::max(1, int(std::lround(m.plotRect.w / logicalRatio))));
		m.plotRect.x = area.x + (area.w - m.plotRect.w) / 2;
		m.plotRect.y = area.y + (area.h - m.plotRect.h) / 2;
		if (_map)
			place(*_map, m.plotRect);
	};
	if (_map)
		configure(MapForm::kLayouts[layout], MapForm::kTabRects[layout], MapForm::kTabCount, MapForm::kToolbar, MapForm::kToolbarCount, MapForm::kActions, MapForm::kActionCount, MapForm::kToolbarRects[layout], MapForm::kActionRects[layout], MapForm::kSummaryRects[layout][0].rect);
	else
		configure(ScanForm::kLayouts[layout], ScanForm::kTabRects[layout], ScanForm::kTabCount, ScanForm::kToolbar, ScanForm::kToolbarCount, ScanForm::kActions, ScanForm::kActionCount, ScanForm::kToolbarRects[layout], ScanForm::kActionRects[layout], ScanForm::kSummaryRects[layout][0].rect);
	m.instance = &_owner;
	m.familyId = MapForm::kFamilyId;
	m.titleText = _title;
	m.mod = getCurrentGame()->getMod();
	m.cutCornerPx = MapForm::kCutCornerPx;
	m.panelFillTop = MapForm::kPanelFillTop;
	m.panelFillBottom = MapForm::kPanelFillBottom;
	m.frameColor = MapForm::kFrame;
	m.selectedTabColor = MapForm::kSelectedTab;
	m.dividerColor = MapForm::kDivider;
	m.textColor = MapForm::kText;
	m.mutedTextColor = MapForm::kMutedText;
	m.scrollTrackColor = MapForm::kPanelFillBottom;
}
void CalypsoTacticalSensor::refresh()
{
	const int w = _map ? 221 : 152, h = _map ? 148 : 152;
	Surface data(w, h);
	auto* view = _map ? static_cast<Surface*>(_map) : static_cast<Surface*>(_scanner);
	if (auto* palette = view->getEffectivePalette())
		data.setPalette(palette);
	if (_map)
		_map->renderMinimap(&data, w, h);
	else
		_scanner->renderScanner(&data);
	std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> rgba(SDL_ConvertSurfaceFormat(data.getSurface(), SDL_PIXELFORMAT_RGBA32, 0), SDL_FreeSurface);
	if (!rgba || !rgba->pixels)
		CalypsoHdUiOverlay::instance().failHdRoute("Tactical sensor image unavailable");
	auto image = std::make_shared<CalypsoHdImageRgba>();
	image->w = w;
	image->h = h;
	image->px.resize(size_t(w) * h * 4);
	for (int y = 0; y < h; ++y)
		memcpy(image->px.data() + size_t(y) * w * 4, static_cast<const Uint8*>(rgba->pixels) + y * rgba->pitch, w * 4);
	static std::uint64_t generation = 0;
	_model.plotImage.source = _map ? "calypso/minimap" : "calypso/scanner";
	_model.plotImage.generation = ++generation;
	_model.plotImage.pixels = std::move(image);
	_model.summary[0].text = _map ? std::string(_owner.tr("STR_LEVEL_SHORT").arg(_camera->getViewLevel())) : "19 × 19";
}
bool CalypsoTacticalSensor::closeRequested(Action* action)
{
	auto* sender = action->getSender();
	if (sender == _buttons.at("done"))
		return true;
	for (const auto& entry : _buttons)
	{
		if (entry.second != sender)
			continue;
		const auto& id = entry.first;
		if (_map)
		{
			if (id == "lower")
				_map->down();
			else if (id == "higher")
				_map->up();
			else if (id == "center")
			{
				if (auto* unit = _battle.getSelectedUnit())
					_camera->centerOnPosition(unit->getPosition());
			}
			else
			{
				auto p = _camera->getCenterPosition();
				p.z = _camera->getViewLevel();
				if (id == "left")
					--p.x;
				else if (id == "right")
					++p.x;
				else if (id == "up")
					--p.y;
				else if (id == "down")
					++p.y;
				_camera->centerOnPosition(p);
			}
		}
		else
		{
			_legend = id == "legend";
			layout();
		}
		refresh();
		break;
	}
	return false;
}
void CalypsoTacticalSensor::collect(CalypsoHdFrameBuilder& builder) const
{
	auto model = _model;
	const auto& metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
	model.projectionScaleX = model.uiScale * metrics.scaleX;
	model.projectionScaleY = model.uiScale * metrics.scaleY;
	model.textRasterScale = std::max(1.0, model.projectionScaleY);
	if (_legend)
	{
		model.summary[0].text = _owner.tr("STR_CAL_SENSOR_SYMBOLS");
	}
	calypsoCollectTabbedManagement(builder, model, _motion);
}
} // namespace Calypso
void MiniMapState::sensorClick(Action* action)
{
	if (_sensor->closeRequested(action))
		btnOkClick(action);
}
void ScannerState::sensorClick(Action* action)
{
	if (_sensor->closeRequested(action))
		exitClick(action);
}
} // namespace OpenXcom
#endif
