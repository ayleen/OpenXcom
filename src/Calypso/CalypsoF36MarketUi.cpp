#ifdef __EMSCRIPTEN__
#include "CalypsoF36MarketUi.h"
#include <algorithm>
#include <cmath>
#include <string>
#include "../Engine/Game.h"
#include "../Engine/Surface.h"
#include "../Engine/Font.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Mod/Mod.h"
#include "CalypsoMarketState.h"
#include "Generated/CalypsoF36Market.generated.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
#include "CalypsoUiMetrics.h"
#include "CalypsoViewportRuntime.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

CalypsoLayoutClass currentLayoutClass()
{
	const CalypsoViewportRuntime& runtime = calypsoViewportRuntime();
	if (!runtime.hasLayout())
		CalypsoHdUiOverlay::instance().failHdRoute("F36 viewport layout is unavailable");
	const CalypsoLayoutMetrics& metrics = runtime.current();
	if (metrics.logicalWidth <= 0 || metrics.logicalHeight <= 0 ||
		metrics.safeWidth <= 0 || metrics.safeHeight <= 0)
		CalypsoHdUiOverlay::instance().failHdRoute("F36 viewport layout is unavailable");
	const CalypsoBaseSafeRect safe{metrics.safeX, metrics.safeY, metrics.safeWidth, metrics.safeHeight};
	return calypsoHarnessEffectiveLayout(calypsoHarnessSession(), safe);
}

template <typename Rect>
CalypsoLogicalRect shiftedRect(const Rect& rect, int dx)
{
	return {rect.x + dx, rect.y, rect.w, rect.h};
}

void applyRect(Surface* surface, const CalypsoLogicalRect& rect)
{
	if (!surface) return;
	surface->setX(rect.x);
	surface->setY(rect.y);
	surface->setWidth(rect.w);
	surface->setHeight(rect.h);
}

int presentationShiftX(int windowDesignW, int windowDesignX, bool wide)
{
	(void)windowDesignW;
	return calypsoHarnessSession().sideBySide && wide ? 40 - windowDesignX : 0;
}

CalypsoLogicalRect touchRect(CalypsoLogicalRect visual)
{
	const int width = std::max(visual.w, CALYPSO_MIN_TOUCH_TARGET);
	const int height = std::max(visual.h, CALYPSO_MIN_TOUCH_TARGET);
	visual.x -= width - visual.w;
	visual.y -= (height - visual.h) / 2;
	visual.w = width;
	visual.h = height;
	return visual;
}

struct Projector
{
	CalypsoLogicalRect window;
	int genWindowX = 0;
	int genWindowY = 0;
	double uiScale = 1.0;
	template <typename Rect>
	CalypsoLogicalRect operator()(const Rect& rect) const
	{
		return {
			window.x + (int)std::llround((rect.x - genWindowX) * uiScale),
			window.y + (int)std::llround((rect.y - genWindowY) * uiScale),
			std::max(1, (int)std::llround(rect.w * uiScale)),
			std::max(1, (int)std::llround(rect.h * uiScale))};
	}
};

template <typename Button>
const Button* findButton(const Button* buttons, int count, const char* id)
{
	for (int i = 0; i < count; ++i)
		if (std::string(buttons[i].id) == id) return &buttons[i];
	return nullptr;
}

template <typename Entry>
CalypsoLogicalRect findRect(const Entry* entries, int count, const char* id, const Projector& project)
{
	for (int i = 0; i < count; ++i)
		if (std::string(entries[i].id) == id) return project(entries[i].rect);
	return {};
}

template <typename Entry>
CalypsoLogicalRect findDesignRect(const Entry* entries, int count, const char* id)
{
	for (int i = 0; i < count; ++i)
		if (std::string(entries[i].id) == id)
			return {entries[i].rect.x, entries[i].rect.y, entries[i].rect.w, entries[i].rect.h};
	return {};
}

/// Conforms the native counterparty list to the generated table geometry so painted
/// cells and native hit-testing agree: design-space columns, minimum row stride
/// from the live small font, and the shared HD scroll seam. Counterparty rows are
/// plain selectable rows: designArrowX must be negative (TextList's disabled
/// sentinel) so quantity arrows are never enabled — enabling them after plain
/// rows already exist would leave TextList::handle indexing arrow buttons that
/// were never created (WASM out-of-bounds).
void conformCounterpartyList(
	TextList* list, Window* window,
	int designViewportW, int designViewportH,
	const int* designCellW, int designArrowX,
	int rowHeight, int visibleRows, int scrollBarWidth, int minThumbHeight,
	Mod* mod, double uiScale)
{
	if (!list || !window || designViewportW <= 0) return;
	list->rebaseNativeSize(designViewportW, designViewportH);
	list->setColumns(2, designCellW[0], designCellW[1]);
	if (designArrowX >= 0)
		list->setArrowColumn(designArrowX, ARROW_VERTICAL);
	if (mod)
	{
		const Font* font = mod->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		list->setMinimumRowHeight(std::max(fontH, rowHeight - spacing));
	}
	const int projectedBar = std::max(1, (int)std::llround(scrollBarWidth * uiScale));
	const int projectedThumb = std::max(1, (int)std::llround(minThumbHeight * uiScale));
	const int rowStride = std::max(1, (int)std::llround(rowHeight * uiScale));
	const size_t slots = visibleRows > 0 ? (size_t)visibleRows : 0;
	list->configureCalypsoHdSelectionList(projectedBar, projectedThumb, rowStride, slots);
}

} // namespace

CalypsoF36MarketUi::~CalypsoF36MarketUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF36MarketUi::topState() const
{
	return _state;
}

bool CalypsoF36MarketUi::suppressLogicalState() const
{
	return false;
}

void CalypsoF36MarketUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtInfo);
	suppression.add(_state->_lstCounterparties);
	suppression.add(_state->_btnCancel);
}

void CalypsoF36MarketUi::collect(CalypsoHdFrameBuilder& builder) const
{
	namespace Gen = CalypsoF36MarketGen;
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F36 market prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F36 market generated layout is missing");

	const CalypsoLogicalRect window = _state->_window
		? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(),
			_state->_window->getWidth(), _state->_window->getHeight()}
		: CalypsoLogicalRect{};
	const double uiScale = generated->window.w > 0
		? (double)window.w / generated->window.w : 1.0;
	const Projector project{window, generated->window.x, generated->window.y, uiScale};

	CalypsoScrollableCollectionModel model{};
	model.familyId = Gen::kFamilyId;
	model.instance = _state;
	model.mod = _state->_game->getMod();
	model.wide = wide;
	model.designWidth = generated->designWidth;
	model.designHeight = generated->designHeight;
	model.window = window;
	model.title = project(generated->title);
	model.summaryBar = project(generated->summaryBar);
	model.headerArt = project(generated->headerArt);
	model.controlBar = project(generated->controlBar);
	model.viewport = project(generated->viewport);
	model.footer = project(generated->footer);
	model.windowWidget = _state->_window;
	model.titleWidget = _state->_txtTitle;
	model.listWidget = _state->_lstCounterparties;
	// The title paints from generated copy (fits the generated title slot in
	// both layouts); the native title widget stays suppressed behavior owner.
	model.titleText = Gen::kTitle;
	// The section label stays contract-owned beside the title:
	// SELECT A COUNTERPARTY paints from generated copy while the native
	// subtitle widget stays suppressed and behavior-owned.
	if (Gen::kHasCollectionHeading == 0 || Gen::kCollectionHeading[0] == '\0')
		CalypsoHdUiOverlay::instance().failHdRoute("F36 market heading contract drifted");
	model.hasHeading = true;
	model.headingText = Gen::kCollectionHeading;
	model.headingRect = project(generated->collectionHeading);
	if (model.headingRect.w <= 0 || model.headingRect.h <= 0)
		CalypsoHdUiOverlay::instance().failHdRoute("F36 market heading geometry is missing");

	const auto* slots = wide ? Gen::kRowSlotsWide : Gen::kRowSlotsCompact;
	const int slotCount = wide ? Gen::kRowSlotWideCount : Gen::kRowSlotCompactCount;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));
	if (Gen::kColumnCount != 2)
		CalypsoHdUiOverlay::instance().failHdRoute("F36 market column contract drifted");
	for (int c = 0; c < Gen::kColumnCount; ++c)
		model.columnLabels.push_back(Gen::kColumns[c].label);
	const auto* headers = wide ? Gen::kColumnHeadersWide : Gen::kColumnHeadersCompact;
	const int headerCount = wide ? Gen::kColumnHeaderWideCount : Gen::kColumnHeaderCompactCount;
	if (headerCount != Gen::kColumnCount)
		CalypsoHdUiOverlay::instance().failHdRoute("F36 market header contract drifted");
	for (int c = 0; c < headerCount; ++c)
	{
		model.columnHeaders.push_back(project(headers[c]));
		model.headerWidgets.push_back(nullptr);
	}
	model.rowCells.resize(model.rowSlots.size());
	const auto* cells = wide ? Gen::kRowCellsWide : Gen::kRowCellsCompact;
	const int cellCount = wide ? Gen::kRowCellWideCount : Gen::kRowCellCompactCount;
	if (cellCount != slotCount * Gen::kColumnCount)
		CalypsoHdUiOverlay::instance().failHdRoute("F36 market cell contract drifted");
	for (int i = 0; i < cellCount; ++i)
		model.rowCells[i / Gen::kColumnCount].push_back(project(cells[i]));

	// The picker carries no totals and no single-mode header art: one form
	// serves both mode titles, so both stay omitted without moving geometry.
	if (Gen::kSummaryCount != 0)
		CalypsoHdUiOverlay::instance().failHdRoute("F36 market summary contract drifted");
	if (generated->hasHeaderArt != 0)
		CalypsoHdUiOverlay::instance().failHdRoute("F36 market header-art contract drifted");
	model.hasHeaderArt = false;

	if (_state->_lstCounterparties)
	{
		const std::size_t total = _state->_lstCounterparties->getTexts();
		for (std::size_t row = 0; row < total; ++row)
		{
			CalypsoScrollableCollectionRow entry{};
			for (int c = 0; c < Gen::kColumnCount; ++c)
				entry.values.push_back(_state->_lstCounterparties->getCellText(row, c));
			model.rows.push_back(entry);
		}
		model.scrollOffset = _state->_lstCounterparties->getScroll();
		const unsigned int selected = _state->_lstCounterparties->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
		if (_state->_lstCounterparties->isCalypsoHdSelectionList())
		{
			const SDL_Rect track = _state->_lstCounterparties->getCalypsoHdTrackRect();
			const SDL_Rect thumb = _state->_lstCounterparties->getCalypsoHdThumbRect();
			model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
			model.nativeTrack = {track.x, track.y, track.w, track.h};
			model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
			model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
		}
	}

	if (!_state->_btnCancel)
		CalypsoHdUiOverlay::instance().failHdRoute("F36 market cancel widget is missing");
	{
		const auto* generatedButton = findButton(Gen::kButtons, Gen::kButtonCount, "cancel");
		if (!generatedButton)
			CalypsoHdUiOverlay::instance().failHdRoute("F36 market button contract drifted");
		CalypsoSmallConfirmationButton button{};
		button.widget = _state->_btnCancel;
		button.text = generatedButton->label;
		const auto* rects = wide ? Gen::kButtonRectsWide : Gen::kButtonRectsCompact;
		const int rectCount = wide ? Gen::kButtonRectWideCount : Gen::kButtonRectCompactCount;
		button.rect = findRect(rects, rectCount, "cancel", project);
		if (button.rect.w <= 0 || button.rect.h <= 0)
			CalypsoHdUiOverlay::instance().failHdRoute("F36 market button geometry is missing");
		button.tone = CalypsoActionTone::Safe;
		button.restFill = generatedButton->fill;
		button.restBorder = generatedButton->border;
		button.textColor = generatedButton->text;
		model.buttons.push_back(button);
	}

	model.cutCornerPx = Gen::kCutCornerPx;
	model.panelFillTop = Gen::kPanelFillTop;
	model.panelFillBottom = Gen::kPanelFillBottom;
	model.frameColor = Gen::kFrame;
	model.dividerColor = Gen::kDivider;
	model.footerDotColor = Gen::kDivider;
	model.textColor = Gen::kText;
	model.mutedTextColor = Gen::kMutedText;
	model.selectionColor = Gen::kSelection;
	model.scrollTrackColor = Gen::kScrollTrack;
	model.scrollThumbColor = Gen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = Gen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.headerHeight = generated->headerHeight;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(generated->minThumbHeight * uiScale));
	model.motionDurationMs = Gen::kMotionDurationMs;
	model.motionScaleFrom = Gen::kMotionScaleFrom;

	calypsoCollectScrollableCollection(builder, model, _motion);
}

void CalypsoF36MarketUi::applyGeneratedLayout(CalypsoMarketState& state, bool wide)
{
	namespace Gen = CalypsoF36MarketGen;
	const auto* generated = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	const int dx = presentationShiftX(generated->window.w, generated->window.x, wide);
	applyRect(state._window, shiftedRect(generated->window, dx));
	applyRect(state._txtTitle, shiftedRect(generated->title, dx));
	applyRect(state._lstCounterparties, shiftedRect(generated->viewport, dx));
	const auto* buttonRects = wide ? Gen::kButtonRectsWide : Gen::kButtonRectsCompact;
	const int buttonRectCount = wide ? Gen::kButtonRectWideCount : Gen::kButtonRectCompactCount;
	applyRect(state._btnCancel, touchRect(shiftedRect(findDesignRect(buttonRects, buttonRectCount, "cancel"), dx)));
	const auto* cells = wide ? Gen::kRowCellsWide : Gen::kRowCellsCompact;
	const int cellCount = wide ? Gen::kRowCellWideCount : Gen::kRowCellCompactCount;
	int designCellW[2] = {50, 50};
	if (cellCount >= 2)
		for (int c = 0; c < 2; ++c)
			designCellW[c] = cells[c].w;
	// No quantity arrows on this list: pass TextList's disabled sentinel.
	conformCounterpartyList(state._lstCounterparties, state._window,
		generated->viewport.w, generated->viewport.h,
		designCellW, -1,
		generated->rowHeight, generated->visibleRows,
		generated->scrollBarWidth, generated->minThumbHeight,
		state._game ? state._game->getMod() : nullptr, 1.0);
}

void CalypsoF36MarketUi::configure(CalypsoMarketState& state)
{
	if (state._hdAdapter)
	{
		CalypsoHdUiOverlay::instance().clearAdapter(state._hdAdapter);
		delete state._hdAdapter;
		state._hdAdapter = nullptr;
		state._hdLayout = false;
	}
	if (!state._game || !state._game->getMod() || !state._game->getLanguage())
		CalypsoHdUiOverlay::instance().failHdRoute("F36 market prerequisites are unavailable");
	if (!state._game->getMod()->isHdUiFamilyEnabled("F36"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;

	state._hdWideLayout = currentLayoutClass() == CalypsoLayoutClass::Wide;
	state._hdHarnessGeneration = Calypso::calypsoHarnessSession().generation;
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF36MarketGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F36 market generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	auto* adapter = new CalypsoF36MarketUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
	if (calypsoHarnessHostUp(calypsoHarnessSession())) calypsoHdHarnessDomShow();
}

bool CalypsoF36MarketUi::resize(CalypsoMarketState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = currentLayoutClass() == CalypsoLayoutClass::Wide;
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF36MarketGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	return true;
}

void CalypsoF36MarketUi::teardown(CalypsoMarketState& state)
{
	calypsoHdHarnessTeardownForTarget(&state, state._hdHarnessGeneration);
	delete state._hdAdapter;
	state._hdAdapter = nullptr;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
