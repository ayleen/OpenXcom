#ifdef __EMSCRIPTEN__
#include "CalypsoMissionOutcome.h"
#include "CalypsoHdHarnessHostState.h"
#include "../Savegame/Base.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Mod/Unit.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Engine/Options.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdUiOverlay.h"
#include "Generated/CalypsoF29Debriefing.generated.h"
#include "../Battlescape/DebriefingState.h"
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
namespace Gen = CalypsoF29DebriefingGen;
namespace
{
CalypsoLogicalRect outcomeRect(const CalypsoLogicalRect &window, const Gen::CalypsoF29DebriefingGenLayout &g,
	const Gen::CalypsoF29DebriefingGenRect &rect)
{
	const auto x = [&](int coordinate) { return window.x + int(std::lround((coordinate - g.window.x) * double(window.w) / g.window.w)); };
	const auto y = [&](int coordinate) { return window.y + int(std::lround((coordinate - g.window.y) * double(window.h) / g.window.h)); };
	return {x(rect.x), y(rect.y), std::max(1, x(rect.x + rect.w) - x(rect.x)),
		std::max(1, y(rect.y + rect.h) - y(rect.y))};
}


const char *const tabKeys[] = {"STR_SCORE","STR_CAL_DEBRIEF_PERSONNEL","STR_CAL_DEBRIEF_RECOVERY"};

void addRow(std::vector<CalypsoTabbedRow> &rows, const std::string &name,
	const std::string &quantity = {}, const std::string &value = {})
{
	rows.push_back({{}, {name, quantity, value}});
}
}

CalypsoMissionOutcome::CalypsoMissionOutcome(DebriefingState &state)
	: _state(state), _generation(calypsoHarnessSession().generation)
{
	for (auto *surface : state._surfaces) surface->setVisible(false);
	auto bind = [&](auto &buttons) {
		for (auto &button : buttons)
		{
			button = new TextButton(1, 1, 0, 0);
			state.add(button, "button", "debriefing");
			button->onMouseClick((ActionHandler)&DebriefingState::hdPageClick);
		}
	};
	bind(_tabs);
	bind(_paging);
	for (int i=0; i<3; ++i) _tabs[i]->onKeyboardPress((ActionHandler)&DebriefingState::hdPageClick, SDLKey(SDLK_1+i));
	_paging[0]->onKeyboardPress((ActionHandler)&DebriefingState::hdPageClick, SDLK_PAGEUP);
	_paging[1]->onKeyboardPress((ActionHandler)&DebriefingState::hdPageClick, SDLK_PAGEDOWN);
	_actions = {state._btnSell,state._btnTransfer,state._btnOk};
	state._btnOk->setText(state.tr("STR_CAL_DEBRIEF_CONTINUE"));
	layout();
	CalypsoHdUiOverlay::instance().registerAdapter(this);
	if (calypsoHarnessHostUp(calypsoHarnessSession())) calypsoHdHarnessDomShow();
}

CalypsoMissionOutcome::~CalypsoMissionOutcome()
{
	calypsoHdHarnessTeardownForTarget(&_state, _generation);
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

void CalypsoMissionOutcome::collectLogicalSuppression(CalypsoHdLogicalSuppression &suppression) const
{
	for (auto *surface : _state._surfaces) suppression.add(surface);
}

void CalypsoMissionOutcome::capture()
{
	int total = 0;
	for (auto *stat : _state._stats)
	{
		if (!stat->qty) continue;
		addRow(_rows[0], _state.tr(stat->item), std::to_string(stat->qty), std::to_string(stat->score));
		if (stat->recovery) addRow(_rows[2], _state.tr(stat->item), std::to_string(stat->qty));
		total += stat->score;
	}
	addRow(_rows[0], _state.tr("STR_TOTAL_UC"), {}, std::to_string(total));
	auto addGain = [&](const std::string &name, const char *label, int delta) {
		if (delta) addRow(_rows[1], name, _state.tr(label), (delta>0?"+":"")+std::to_string(delta));
	};
	for (const auto &entry : _state._soldierStats)
	{
		const auto &s=entry.second;
		const size_t before=_rows[1].size();
		addGain(entry.first,"STR_TIME_UNITS",s.tu);
		addGain(entry.first,"STR_STAMINA",s.stamina);
		addGain(entry.first,"STR_HEALTH",s.health);
		addGain(entry.first,"STR_BRAVERY",s.bravery);
		addGain(entry.first,"STR_REACTIONS",s.reactions);
		addGain(entry.first,"STR_FIRING_ACCURACY",s.firing);
		addGain(entry.first,"STR_THROWING_ACCURACY",s.throwing);
		addGain(entry.first,"STR_MELEE_ACCURACY",s.melee);
		addGain(entry.first,"STR_STRENGTH",s.strength);
		const bool mana=_state._game->getMod()->isManaFeatureEnabled();
		addGain(entry.first,mana?"STR_MANA_POOL":"STR_PSIONIC_STRENGTH",mana?s.mana:s.psiStrength);
		addGain(entry.first,"STR_PSIONIC_SKILL",s.psiSkill);
		if (before==_rows[1].size()) addRow(_rows[1],entry.first,{}, "0");
	}
	if (_rows[1].empty()) addRow(_rows[1], _state.tr("STR_CAL_DEBRIEF_EMPTY"));
	for (const auto &type : _state._game->getMod()->getItemsList())
	{
		const auto *rule = _state._game->getMod()->getItem(type);
		const int count = _state.getRecoveredItemCount(rule);
		if (count>0) _initialStock.emplace_back(rule, count);
	}
	_evidenceRows = _rows[2].size();
	refresh();
}

void CalypsoMissionOutcome::refresh()
{
	_rows[2].resize(_evidenceRows);
	if (!_initialStock.empty()) addRow(_rows[2], _state.tr("STR_CAL_DEBRIEF_RECEIVED"));
	for (const auto &item : _initialStock)
		addRow(_rows[2], _state.tr(item.first->getType()), std::to_string(item.second),
			std::to_string(_state.getRecoveredItemCount(item.first)));
	if (_rows[2].empty()) addRow(_rows[2], _state.tr("STR_CAL_DEBRIEF_EMPTY"));
	auto *base = _state._base;
	if (base && !_state._destroyBase)
	{
		if (Options::storageLimitsEnforced && base->storesOverfull())
			addRow(_rows[2],_state.tr("STR_STORAGE_EXCEEDED").arg(base->getName()));
		for (const auto &prison : _state._containmentStateInfo)
		{
			int available = base->getAvailableContainment(prison.first), used = base->getUsedContainment(prison.first);
			if (prison.second==1 || (prison.second==2 && used>available))
				addRow(_rows[2],_state.trAlt("STR_CONTAINMENT_EXCEEDED",prison.first).arg(base->getName()),
					std::to_string(used),std::to_string(available));
		}
	}
	layout();
}

void CalypsoMissionOutcome::layout()
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

	const auto position = [&](TextButton *button, const auto &design, bool visible) {
		const auto r = outcomeRect(_window,g,design.rect);
		button->setVisible(visible);
		button->setX(r.x); button->setY(r.y); button->setWidth(r.w); button->setHeight(r.h);
	};
	const size_t count = _rows[_tab].size(), perPage = std::max(1,g.visibleRows);
	_page = std::min(_page, count ? (count-1)/perPage : size_t(0));
	for (size_t i=0;i<_tabs.size();++i) position(_tabs[i],Gen::kTabRects[_layout][i],true);
	position(_paging[0],Gen::kToolbarRects[_layout][0],_page>0);
	position(_paging[1],Gen::kToolbarRects[_layout][1],(_page+1)*perPage<count);
	const bool canDispose = _tab==2 && _state._base && !_state._destroyBase && _state._showSellButton && _state.getTotalRecoveredItemCount()>0;
	position(_actions[0],Gen::kActionRects[_layout][0],canDispose);
	position(_actions[1],Gen::kActionRects[_layout][1],canDispose && _state._game->getSavedGame()->getBases()->size()>1);
	position(_actions[2],Gen::kActionRects[_layout][2],true);
}

void CalypsoMissionOutcome::click(Action *action)
{
	for (size_t i=0;i<_tabs.size();++i) if (action->getSender()==_tabs[i])
	{
		_tab=int(i); _page=0; layout(); return;
	}
	if (action->getSender()==_paging[0] && _page) --_page;
	else if (action->getSender()==_paging[1]) ++_page;
	layout();
}

void CalypsoMissionOutcome::collect(CalypsoHdFrameBuilder &builder) const
{
	const auto &g = Gen::kLayouts[_layout];
	const auto &metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
	const double scale = double(_window.w) / g.window.w;
	auto project = [&](const auto &rect) { return outcomeRect(_window, g, rect); };
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
	model.windowWidget = _state._window;
	model.titleWidget = _state._txtTitle;
	model.listWidget = _state._lstStats;
	model.titleText = _state._txtTitle->getText();
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


	model.summary.push_back({_state._txtRating->getText(), project(Gen::kSummaryRects[_layout][0].rect)});
	for (size_t i=0;i<_tabs.size();++i)
		model.tabs.push_back({Gen::kTabIds[i],tr(tabKeys[i]),int(i)==_tab,true,_tabs[i],project(Gen::kTabRects[_layout][i].rect)});
	const char *headers[][3] = {
		{"STR_ITEM","STR_QUANTITY","STR_SCORE"},
		{"STR_NAME","STR_CAL_DEBRIEF_ATTRIBUTE","STR_CAL_DEBRIEF_GAIN"},
		{"STR_ITEM","STR_QUANTITY","STR_CAL_DEBRIEF_AVAILABLE"}
	};
	for (int i=0;i<3;++i)
		model.columns.push_back({Gen::kColumnHeaders[_layout][i].id,tr(headers[_tab][i]),project(Gen::kColumnHeaders[_layout][i].rect)});
	for (int slot=0;slot<g.visibleRows;++slot)
	{
		const size_t index = _page*g.visibleRows+slot;
		if (index >= _rows[_tab].size()) break;
		model.rowSlots.push_back(project(Gen::kRowSlots[_layout][slot].rect));
		model.rows.push_back(_rows[_tab][index]);
	}
	const auto button = [&](auto &list, TextButton *widget, const auto &spec, const auto &box, const std::string &text) {
		if (widget->getVisible()) list.push_back({widget,nullptr,text,project(box.rect),calypsoTabbedToneForContract(spec.tone),spec.fill,spec.border,spec.text});
	};
	for (size_t i=0;i<_actions.size();++i) button(model.actions,_actions[i],Gen::kActions[i],Gen::kActionRects[_layout][i],_actions[i]->getText());
	for (size_t i=0;i<_paging.size();++i) button(model.toolbar,_paging[i],Gen::kToolbar[i],Gen::kToolbarRects[_layout][i],tr(i?"STR_CAL_DEBRIEF_NEXT":"STR_CAL_DEBRIEF_PREVIOUS"));
	calypsoCollectTabbedManagement(builder,model,_motion);
}
} }
#endif
