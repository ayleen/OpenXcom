#ifdef __EMSCRIPTEN__
#include "CalypsoStrategicList.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoViewportRuntime.h"
#include "CalypsoHdHarnessHostState.h"
#include "Generated/CalypsoF16ExtendedLinks.generated.h"
#include "Generated/CalypsoF16MusicSelector.generated.h"
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
		action.text = action.widget->getText();
		action.tone = primary ? CalypsoActionTone::Primary : CalypsoActionTone::Safe;
		action.restFill = buttons[b].fill;
		action.restBorder = buttons[b].border;
		action.textColor = buttons[b].text;
		for (int j = 0; j < buttonCount; ++j)
			if (std::string(buttons[b].id) == buttonRects[j].id) action.rect = rect(buttonRects[j].rect);
		if (action.rect.w <= 0) CalypsoHdUiOverlay::instance().failHdRoute("F16 action geometry is missing");
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
void music(CalypsoScrollableCollectionModel &m, bool wide, TextButton *cancel, TextButton *keep)
{
	namespace G = CalypsoF16MusicSelectorGen;
	const auto *g = G::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) CalypsoHdUiOverlay::instance().failHdRoute("F16 table layout is missing");
	readTable(m, *g, wide ? G::kRowSlotsWide : G::kRowSlotsCompact,
		wide ? G::kRowCellsWide : G::kRowCellsCompact, wide ? G::kColumnHeadersWide : G::kColumnHeadersCompact,
		G::kColumns, G::kColumnCount, G::kButtons, wide ? G::kButtonRectsWide : G::kButtonRectsCompact,
		G::kButtonCount, cancel, keep);
	m.familyId = G::kFamilyId;
	m.hasHeading = G::kHasCollectionHeading;
	m.headingText = G::kCollectionHeading;
	m.cutCornerPx = G::kCutCornerPx;
	m.panelFillTop = G::kPanelFillTop;
	m.panelFillBottom = G::kPanelFillBottom;
	m.frameColor = G::kFrame;
	m.dividerColor = G::kDivider;
	m.footerDotColor = G::kDivider;
	m.textColor = G::kText;
	m.mutedTextColor = G::kMutedText;
	m.selectionColor = G::kSelection;
	m.scrollTrackColor = G::kScrollTrack;
	m.scrollThumbColor = G::kScrollThumb;
	m.motionDurationMs = G::kMotionDurationMs;
	m.motionScaleFrom = G::kMotionScaleFrom;
	m.visualScale = G::kPresentationScale;
}
void destinations(CalypsoScrollableCollectionModel &m, bool wide, TextButton *cancel, TextButton *keep)
{
	namespace G = CalypsoF16ExtendedLinksGen;
	const auto *g = G::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) CalypsoHdUiOverlay::instance().failHdRoute("F16 table layout is missing");
	readTable(m, *g, wide ? G::kRowSlotsWide : G::kRowSlotsCompact,
		wide ? G::kRowCellsWide : G::kRowCellsCompact, wide ? G::kColumnHeadersWide : G::kColumnHeadersCompact,
		G::kColumns, G::kColumnCount, G::kButtons, wide ? G::kButtonRectsWide : G::kButtonRectsCompact,
		G::kButtonCount, cancel, keep);
	m.familyId = G::kFamilyId;
	m.hasHeading = G::kHasCollectionHeading;
	m.headingText = G::kCollectionHeading;
	m.cutCornerPx = G::kCutCornerPx;
	m.panelFillTop = G::kPanelFillTop;
	m.panelFillBottom = G::kPanelFillBottom;
	m.frameColor = G::kFrame;
	m.dividerColor = G::kDivider;
	m.footerDotColor = G::kDivider;
	m.textColor = G::kText;
	m.mutedTextColor = G::kMutedText;
	m.selectionColor = G::kSelection;
	m.scrollTrackColor = G::kScrollTrack;
	m.scrollThumbColor = G::kScrollThumb;
	m.motionDurationMs = G::kMotionDurationMs;
	m.motionScaleFrom = G::kMotionScaleFrom;
	m.visualScale = G::kPresentationScale;
}
} // namespace

CalypsoStrategicList::CalypsoStrategicList(State *owner, const State *underlay, Mod *mod,
	Window *window, Text *title, TextList *list, TextButton *cancel, TextButton *keep)
	: _owner(owner), _underlay(underlay), _mod(mod), _window(window), _title(title),
	  _list(list), _cancel(cancel), _keep(keep)
{
	if (!_owner || !_mod || !_window || !_title || !_list || !_cancel)
		CalypsoHdUiOverlay::instance().failHdRoute("F16 table prerequisites are unavailable");
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
	if (!viewport.hasLayout()) CalypsoHdUiOverlay::instance().failHdRoute("F16 viewport is unavailable");
	const auto &v = viewport.current();
	const bool wide = calypsoHarnessEffectiveLayout(calypsoHarnessSession(), {v.safeX, v.safeY, v.safeWidth, v.safeHeight}) == CalypsoLayoutClass::Wide;
	_design = {};
	_design.wide = wide;
	if (_keep) music(_design, wide, _cancel, _keep);
	else destinations(_design, wide, _cancel, _keep);
	const auto &d = _design;
	place(_window, d.window);
	place(_title, d.title);
	for (const auto &action : d.buttons) place(action.widget, action.rect);
	const int height = d.rowHeight * d.visibleRows;
	place(_list, {d.viewport.x, d.rowSlots.front().y, d.viewport.w, height});
	_list->rebaseNativeSize(d.viewport.w, height);
	_list->setColumns(2, d.rowCells.front()[0].w, d.rowCells.front()[1].w);
	auto *font = _mod->getFont("FONT_SMALL");
	_list->setMinimumRowHeight(std::max(font->getHeight(), d.rowHeight - font->getSpacing()));
	_list->configureCalypsoHdSelectionList(d.scrollBarWidth, d.minThumbHeight, d.rowHeight, 0, height, d.visibleRows);
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
	m.hasSelection = m.selectedRow < m.rows.size();
	m.scrollOffset = _list->getScroll();
	m.nativeTrack = rect(_list->getCalypsoHdTrackRect());
	m.nativeThumb = rect(_list->getCalypsoHdThumbRect());
	m.hasNativeScrollGeometry = m.nativeTrack.h > 0;
	m.nativeThumbVisible = m.nativeThumb.h > 0;
	calypsoCollectScrollableCollection(builder, m, _motion);
}
} }
#endif
