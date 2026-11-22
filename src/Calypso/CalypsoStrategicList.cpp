#ifdef __EMSCRIPTEN__
#include "CalypsoStrategicList.h"
#include "CalypsoFittedTableRows.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoViewportRuntime.h"
#include "CalypsoHdHarnessHostState.h"
#include "Generated/CalypsoF16ExtendedLinks.generated.h"
#include "Generated/CalypsoF16MusicSelector.generated.h"
#include "Generated/CalypsoF30NoExperience.generated.h"
#include "Generated/CalypsoF30TurnDiary.generated.h"
#include "Generated/CalypsoF28TacticalLinks.generated.h"
#include "Generated/CalypsoF28TurnTransition.generated.h"
#include "Generated/CalypsoF27TargetBriefing.generated.h"
#include "Generated/CalypsoF27MissionBriefing.generated.h"
#include "Generated/CalypsoF30Promotions.generated.h"
#include "Generated/CalypsoF30FallenCommendations.generated.h"
#include "Generated/CalypsoF30Commendations.generated.h"
#include "Generated/CalypsoF30ExperienceOverview.generated.h"
#include "Generated/CalypsoHdTheme.generated.h"
#include "../Engine/State.h"
#include "../Engine/Font.h"
#include "../Mod/Mod.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Interface/TextButton.h"
namespace OpenXcom { namespace Calypso {
namespace {
template<class Rect> CalypsoLogicalRect rect(const Rect &r) { return {r.x,r.y,r.w,r.h}; }

template<class Layout, class Slot, class Button, class ButtonRect, class Column>
void readTable(CalypsoScrollableCollectionModel &m, const Layout &g, const Slot *slots,
	const Slot *cells, const Slot *headers, const Column *columns, int columnCount,
	const Button *buttons, const ButtonRect *buttonRects, int buttonCount,
	TextButton *cancel, TextButton *keep)
{
	m.designWidth = g.designWidth;
	m.designHeight = g.designHeight;
	m.window = rect(g.window);
	m.title = rect(g.title);
	m.titleDesignHeight = g.title.h;
	m.controlBar = rect(g.controlBar);
	m.headingRect = rect(g.collectionHeading);
	m.summaryBar = rect(g.summaryBar);
	m.headerArt = rect(g.headerArt);
	m.viewport = rect(g.viewport);
	m.footer = rect(g.footer);
	m.visibleRows = g.visibleRows;
	m.rowHeight = g.rowHeight;
	m.headerHeight = g.headerHeight;
	m.scrollBarWidth = g.scrollBarWidth;
	m.minThumbHeight = g.minThumbHeight;
	for (int c = 0; c < columnCount; ++c)
	{
		m.columnLabels.push_back(columns[c].label);
		m.columnHeaders.push_back(rect(headers[c]));
		m.headerWidgets.push_back(nullptr);
	}
	for (int r = 0; r < g.visibleRows; ++r)
	{
		m.rowSlots.push_back(rect(slots[r]));
		m.rowCells.emplace_back();
		for (int c = 0; c < columnCount; ++c)
			m.rowCells.back().push_back(rect(cells[r * columnCount + c]));
	}
	for (int b = 0; b < buttonCount; ++b)
	{
		const bool primary = std::string(buttons[b].id) == "keep";
		CalypsoSmallConfirmationButton action;
		action.widget = primary ? keep : cancel;
		if (!action.widget || !action.widget->getVisible()) continue;
		action.text = action.widget->getText();
		action.tone = primary ? CalypsoActionTone::Primary : CalypsoActionTone::Safe;
		action.restFill = buttons[b].fill;
		action.restBorder = buttons[b].border;
		action.textColor = buttons[b].text;
		for (int j = 0; j < buttonCount; ++j)
			if (std::string(buttons[b].id) == buttonRects[j].id) action.rect = rect(buttonRects[j].rect);
		if (action.rect.w <= 0) CalypsoHdUiOverlay::instance().failHdRoute("HD list action geometry is missing");
		m.buttons.push_back(action);
	}
}

void place(Surface *widget, CalypsoLogicalRect r)
{
	widget->setX(r.x);
	widget->setY(r.y);
	widget->setWidth(r.w);
	widget->setHeight(r.h);
}
} // namespace

CalypsoStrategicList::CalypsoStrategicList(State *owner, const State *underlay, Mod *mod,
	Window *window, Text *title, TextList *list, TextButton *cancel, Content content, TextButton *keep)
	: _owner(owner), _underlay(underlay), _mod(mod), _window(window), _title(title),
	  _list(list), _cancel(cancel), _keep(keep), _content(content)
{
	if (!_owner || !_mod || !_window || !_title || !_list || !_cancel)
		CalypsoHdUiOverlay::instance().failHdRoute("HD list prerequisites are unavailable");
	_harnessGeneration = calypsoHarnessSession().generation;
	layout();
	CalypsoHdUiOverlay::instance().registerAdapter(this);
	if (calypsoHarnessHostUp(calypsoHarnessSession())) calypsoHdHarnessDomShow();
}

CalypsoStrategicList::~CalypsoStrategicList()
{
	calypsoHdHarnessTeardownForTarget(_owner, _harnessGeneration);
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

void CalypsoStrategicList::layout()
{
	const auto &viewport = calypsoViewportRuntime();
	if (!viewport.hasLayout()) CalypsoHdUiOverlay::instance().failHdRoute("HD list viewport is unavailable");
	const auto &v = viewport.current();
	const bool wide = calypsoHarnessEffectiveLayout(calypsoHarnessSession(), {v.safeX, v.safeY, v.safeWidth, v.safeHeight}) == CalypsoLayoutClass::Wide;
	_design = {};
	_design.wide = wide;
	auto read = [&](const auto &... fields) { readTable(_design, fields..., _cancel, _keep); };
	bool found = false;
	switch (_content)
	{
	case Content::Tracks: found = CalypsoF16MusicSelectorGen::bindTable(wide, _design, read); break;
	case Content::Destinations: found = CalypsoF16ExtendedLinksGen::bindTable(wide, _design, read); break;
	case Content::ExperienceOverview:
		found = CalypsoF30ExperienceOverviewGen::bindTable(wide, _design, read);
		_design.columnLabels = {_owner->tr("STR_NAME"), _owner->tr("STR_BRAVERY_ABBREVIATION"),
			_owner->tr("STR_REACTIONS_ABBREVIATION"), _owner->tr("STR_FIRING_ACCURACY_ABBREVIATION"),
			_owner->tr("STR_THROWING_ACCURACY_ABBREVIATION"), _owner->tr("STR_PSIONIC_SKILL_ABBREVIATION"),
			_owner->tr("STR_PSIONIC_STRENGTH_ABBREVIATION"), _owner->tr("STR_MELEE_ACCURACY_ABBREVIATION"),
			_mod->isManaFeatureEnabled() && _mod->isManaTrainingPrimary() ? std::string(_owner->tr("STR_MANA_ABBREVIATION")) : ""};
		break;
	case Content::Commendations:
		found = CalypsoF30CommendationsGen::bindTable(wide, _design, read);
		_design.columnLabels = {_owner->tr("STR_MEDALS"), _owner->tr("STR_MEDAL_DECOR_LEVEL")};
		break;
	case Content::FallenCommendations:
		found = CalypsoF30FallenCommendationsGen::bindTable(wide, _design, read);
		_design.columnLabels = {_owner->tr("STR_NAME"), _owner->tr("STR_RANK"), _owner->tr("STR_STATISTICS")};
		break;
	case Content::Promotions:
		found = CalypsoF30PromotionsGen::bindTable(wide, _design, read);
		_design.columnLabels = {_owner->tr("STR_NAME"), _owner->tr("STR_NEW_RANK"), _owner->tr("STR_BASE")};
		break;
	case Content::MissionBriefing:
		found = CalypsoF27MissionBriefingGen::bindTable(wide, _design, read);
		if (found) _design.columnLabels.front() = _owner->tr("STR_BRIEFING");
		break;
	case Content::TargetBriefing:
		found = CalypsoF27TargetBriefingGen::bindTable(wide, _design, read);
		if (found) _design.columnLabels.front() = _owner->tr("STR_BRIEFING");
		break;
	case Content::TurnTransition:
		found = CalypsoF28TurnTransitionGen::bindTable(wide, _design, read);
		if (found) _design.columnLabels.front() = _owner->tr("STR_CAL_BATTLE_MESSAGE");
		break;
	case Content::TacticalLinks:
		found = CalypsoF28TacticalLinksGen::bindTable(wide, _design, read);
		_design.columnLabels = {_owner->tr("STR_EXTENDED_LINKS"), _owner->tr("STR_CAL_BATTLE_CONTEXT")};
		break;
	case Content::TurnDiary:
		found = CalypsoF30TurnDiaryGen::bindTable(wide, _design, read);
		if (found) _design.columnLabels.front() = _owner->tr("STR_HIT_LOG");
		break;
	case Content::ExperienceRoster:
		found = CalypsoF30NoExperienceGen::bindTable(wide, _design, read);
		if (found) _design.columnLabels.front() = _owner->tr("STR_NAME");
		break;
	}
	if (!found) CalypsoHdUiOverlay::instance().failHdRoute("HD list layout is unavailable");
	const auto &d = _design;
	place(_window, d.window);
	place(_title, d.title);
	for (const auto &action : d.buttons) place(action.widget, action.rect);
	const int height = d.rowHeight * d.visibleRows;
	place(_list, {d.viewport.x, d.rowSlots.front().y, d.viewport.w, height});
	_list->rebaseNativeSize(d.viewport.w, height);
	std::vector<size_t> widths;
	for (const auto &cell : d.rowCells.front()) widths.push_back(cell.w);
	_list->setColumnWidths(widths);
	auto *font = _mod->getFont("FONT_SMALL");
	_list->setMinimumRowHeight(std::max(font->getHeight(), d.rowHeight - font->getSpacing()));
}

int CalypsoStrategicList::bodyFontSize() const
{
	const float scale = _design.wide ? CalypsoHdThemeGen::kBodyFontSizeScaleWide : CalypsoHdThemeGen::kBodyFontSizeScaleCompact;
	return std::lround(CalypsoHdThemeGen::kBodyFontSizePx * scale);
}

void CalypsoStrategicList::fitInput()
{
	const auto &d = _design;
	calypsoFitTableHitRows(*_list, d.viewport.w, d.visibleRows, d.scrollBarWidth, d.minThumbHeight);
}

void CalypsoStrategicList::collect(CalypsoHdFrameBuilder &builder) const
{
	auto m = _design;
	const auto scale = double(_window->getWidth()) / m.window.w;
	auto project = [&](CalypsoLogicalRect &r) {
		r = {_window->getX() + int(std::lround((r.x - _design.window.x) * scale)),
			_window->getY() + int(std::lround((r.y - _design.window.y) * scale)),
			int(std::lround(r.w * scale)), int(std::lround(r.h * scale))};
	};
	for (auto *r : {&m.window, &m.title, &m.headingRect, &m.footer, &m.viewport, &m.controlBar, &m.headerArt, &m.summaryBar}) project(*r);
	for (auto &r : m.columnHeaders) project(r);
	for (auto &r : m.rowSlots) project(r);
	for (auto &row : m.rowCells) for (auto &r : row) project(r);
	for (auto &button : m.buttons) project(button.rect);
	m.mod = _mod;
	m.instance = _owner;
	m.titleText = _title->getText();
	m.windowWidget = _window;
	m.titleWidget = _title;
	m.listWidget = _list;
	m.uiScale = scale;
	const auto &metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
	m.projectionScaleX = scale * metrics.scaleX;
	m.projectionScaleY = scale * metrics.scaleY;
	for (size_t i = 0; i < _list->getTexts(); ++i)
	{
		m.rows.emplace_back();
		for (size_t c = 0; c < m.columnLabels.size(); ++c) m.rows.back().values.push_back(_list->getCellText(i,c));
	}
	m.selectedRow = _list->getSelectedRow();
	const bool article = _articleIds && m.selectedRow < _articleIds->size() && !(*_articleIds)[m.selectedRow].empty();
	m.hasSelection = m.selectedRow < m.rows.size() && (article || _content == Content::Tracks
		|| _content == Content::Destinations || _content == Content::TacticalLinks || _content == Content::ExperienceOverview);
	m.scrollOffset = _list->getScroll();
	m.nativeTrack = rect(_list->getCalypsoHdTrackRect());
	m.nativeThumb = rect(_list->getCalypsoHdThumbRect());
	m.hasNativeScrollGeometry = m.nativeTrack.h > 0;
	m.nativeThumbVisible = m.nativeThumb.h > 0;
	calypsoCollectScrollableCollection(builder, m, _motion);
}
} }
#endif
