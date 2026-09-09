#ifdef __EMSCRIPTEN__
#include "CalypsoF12TransferUi.h"
#include <algorithm>
#include <cmath>
#include <string>
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Engine/Font.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextEdit.h"
#include "../Interface/ComboBox.h"
#include "../Interface/TextList.h"
#include "../Basescape/TransferBaseState.h"
#include "../Basescape/TransferItemsState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF12TransferBase.generated.h"
#include "Generated/CalypsoF12TransferItems.generated.h"
#include "CalypsoCollectionInteraction.h"
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
		CalypsoHdUiOverlay::instance().failHdRoute("F12 viewport layout is unavailable");
	const CalypsoLayoutMetrics& metrics = runtime.current();
	if (metrics.logicalWidth <= 0 || metrics.logicalHeight <= 0 ||
		metrics.safeWidth <= 0 || metrics.safeHeight <= 0)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 viewport layout is unavailable");
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

/// Native "LABEL>{ALT}value" totals carry their label inline in every locale.
/// The summary slot owns the label; the value is whatever follows the first
/// '>' with the ALT marker stripped. Presentation parsing only, no gameplay.
std::string splitSummaryValue(const std::string& text)
{
	std::string value = text;
	const std::string::size_type cut = value.find('>');
	if (cut != std::string::npos) value = value.substr(cut + 1);
	return calypsoStripPresentationControls(value);
}
template <typename Entry>
CalypsoLogicalRect findDesignRect(const Entry* entries, int count, const char* id)
{
	for (int i = 0; i < count; ++i)
		if (std::string(entries[i].id) == id)
			return {entries[i].rect.x, entries[i].rect.y, entries[i].rect.w, entries[i].rect.h};
	return {};
}

/// Conforms the native transfer list to the generated table geometry so painted
/// cells and native hit-testing agree: design-space columns/arrow, minimum
/// row stride from the live small font, and the shared HD scroll seam.
/// designArrowX < 0 (TextList's disabled sentinel) leaves quantity arrows off:
/// enabling them after plain rows already exist would leave TextList::handle
/// indexing arrow buttons that were never created (WASM out-of-bounds).
/// TransferItems keeps its native quantity-arrow geometry; the destination
/// picker passes -1 because its rows are plain selectable rows.
void conformTransferList(
	TextList* list, Window* window,
	int designViewportW, int designViewportH,
	const int* designCellW, int columnCount, int designArrowX,
	int rowHeight, int visibleRows, int scrollBarWidth, int minThumbHeight,
	Mod* mod, double uiScale)
{
	if (!list || !window || designViewportW <= 0 || columnCount <= 0) return;
	list->rebaseNativeSize(designViewportW, designViewportH);
	if (columnCount == 2)
		list->setColumns(2, designCellW[0], designCellW[1]);
	else
		list->setColumns(4, designCellW[0], designCellW[1], designCellW[2], designCellW[3]);
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

CalypsoF12TransferBaseUi::~CalypsoF12TransferBaseUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF12TransferBaseUi::topState() const
{
	return _state;
}

bool CalypsoF12TransferBaseUi::suppressLogicalState() const
{
	// The destination picker has no toolbar inputs; every claimed widget is
	// suppressed explicitly below, so the row click stays the native owner.
	return false;
}

void CalypsoF12TransferBaseUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtFunds);
	suppression.add(_state->_txtName);
	suppression.add(_state->_txtArea);
	suppression.add(_state->_lstBases);
	suppression.add(_state->_btnCancel);
}

void CalypsoF12TransferBaseUi::collect(CalypsoHdFrameBuilder& builder) const
{
	namespace Gen = CalypsoF12TransferBaseGen;
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 destination prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 destination generated layout is missing");

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
	model.listWidget = _state->_lstBases;
	model.titleText = Gen::kTitle;

	const auto* slots = wide ? Gen::kRowSlotsWide : Gen::kRowSlotsCompact;
	const int slotCount = wide ? Gen::kRowSlotWideCount : Gen::kRowSlotCompactCount;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));
	if (Gen::kColumnCount != 2)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 destination column contract drifted");
	for (int c = 0; c < Gen::kColumnCount; ++c)
		model.columnLabels.push_back(Gen::kColumns[c].label);
	const auto* headers = wide ? Gen::kColumnHeadersWide : Gen::kColumnHeadersCompact;
	const int headerCount = wide ? Gen::kColumnHeaderWideCount : Gen::kColumnHeaderCompactCount;
	if (headerCount != Gen::kColumnCount)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 destination header contract drifted");
	Text* headerWidgets[2] = {_state->_txtName, _state->_txtArea};
	for (int c = 0; c < headerCount; ++c)
	{
		model.columnHeaders.push_back(project(headers[c]));
		model.headerWidgets.push_back(c < 2 ? headerWidgets[c] : nullptr);
	}
	model.rowCells.resize(model.rowSlots.size());
	const auto* cells = wide ? Gen::kRowCellsWide : Gen::kRowCellsCompact;
	const int cellCount = wide ? Gen::kRowCellWideCount : Gen::kRowCellCompactCount;
	if (cellCount != slotCount * Gen::kColumnCount)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 destination cell contract drifted");
	for (int i = 0; i < cellCount; ++i)
		model.rowCells[i / Gen::kColumnCount].push_back(project(cells[i]));

	if (!_state->_txtFunds)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 destination funds widget is missing");
	const auto* summaryRects = wide ? Gen::kSummaryWide : Gen::kSummaryCompact;
	const int summaryRectCount = wide ? Gen::kSummaryWideCount : Gen::kSummaryCompactCount;
	if (Gen::kSummaryCount != 1 || summaryRectCount != 1)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 destination summary contract drifted");
	{
		CalypsoScrollableCollectionSummary entry{};
		entry.label = Gen::kSummary[0].label;
		entry.value = splitSummaryValue(_state->_txtFunds->getText());
		entry.field = project(summaryRects[0].field);
		entry.labelRect = project(summaryRects[0].label);
		entry.valueRect = project(summaryRects[0].value);
		model.summaries.push_back(entry);
	}

	model.hasHeading = Gen::kHasCollectionHeading != 0;
	model.headingText = Gen::kCollectionHeading;
	model.headingRect = project(generated->collectionHeading);

	model.hasHeaderArt = generated->hasHeaderArt != 0;
	model.headerArtPath = Gen::kHeaderArtVfsPath;
	model.headerArtOpacity = std::max(1, std::min(100, (int)Gen::kHeaderArtOpacityPct)) / 100.0f;
	model.headerArtScrim = Gen::kHeaderArtScrim;

	if (_state->_lstBases)
	{
		const std::size_t total = _state->_lstBases->getTexts();
		for (std::size_t row = 0; row < total; ++row)
		{
			CalypsoScrollableCollectionRow entry{};
			for (int c = 0; c < Gen::kColumnCount; ++c)
				entry.values.push_back(_state->_lstBases->getCellText(row, c));
			model.rows.push_back(entry);
		}
		model.scrollOffset = _state->_lstBases->getScroll();
		const unsigned int selected = _state->_lstBases->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
		if (_state->_lstBases->isCalypsoHdSelectionList())
		{
			const SDL_Rect track = _state->_lstBases->getCalypsoHdTrackRect();
			const SDL_Rect thumb = _state->_lstBases->getCalypsoHdThumbRect();
			model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
			model.nativeTrack = {track.x, track.y, track.w, track.h};
			model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
			model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
		}
	}

	struct ButtonBinding { TextButton* widget; const char* id; };
	const ButtonBinding bindings[] = {{_state->_btnCancel, "cancel"}};
	for (const auto& binding : bindings)
	{
		if (!binding.widget || !binding.widget->getVisible()) continue;
		const auto* generatedButton = findButton(Gen::kButtons, Gen::kButtonCount, binding.id);
		if (!generatedButton)
			CalypsoHdUiOverlay::instance().failHdRoute("F12 destination button contract drifted");
		CalypsoSmallConfirmationButton button{};
		button.widget = binding.widget;
		button.text = generatedButton->label;
		const auto* rects = wide ? Gen::kButtonRectsWide : Gen::kButtonRectsCompact;
		const int rectCount = wide ? Gen::kButtonRectWideCount : Gen::kButtonRectCompactCount;
		button.rect = findRect(rects, rectCount, binding.id, project);
		if (button.rect.w <= 0 || button.rect.h <= 0)
			CalypsoHdUiOverlay::instance().failHdRoute("F12 destination button geometry is missing");
		button.tone = std::string(generatedButton->tone) == "primary"
			? CalypsoActionTone::Primary
			: (std::string(generatedButton->tone) == "warning"
				? CalypsoActionTone::Destructive : CalypsoActionTone::Safe);
		button.restFill = generatedButton->fill;
		button.restBorder = generatedButton->border;
		button.textColor = generatedButton->text;
		model.buttons.push_back(button);
	}
	for (auto& button : model.buttons)
		button.peer = model.buttons.size() > 1
			? (button.widget == model.buttons.front().widget
				? model.buttons.back().widget : model.buttons.front().widget)
			: nullptr;

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

void CalypsoF12TransferBaseUi::applyGeneratedLayout(TransferBaseState& state, bool wide)
{
	namespace Gen = CalypsoF12TransferBaseGen;
	const auto* generated = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	const int dx = presentationShiftX(generated->window.w, generated->window.x, wide);
	applyRect(state._window, shiftedRect(generated->window, dx));
	applyRect(state._txtTitle, shiftedRect(generated->title, dx));
	const auto& rowHit = wide ? Gen::kRowHitWide : Gen::kRowHitCompact;
	// Native input starts at the first painted row but spans the full
	// viewport width: the reserved right rail carries the scrollbar track,
	// so the track can never overlap row content.
	applyRect(state._lstBases, {rowHit.x + dx, rowHit.y, generated->viewport.w, rowHit.h});
	const auto* buttonRects = wide ? Gen::kButtonRectsWide : Gen::kButtonRectsCompact;
	const int buttonRectCount = wide ? Gen::kButtonRectWideCount : Gen::kButtonRectCompactCount;
	applyRect(state._btnCancel, touchRect(shiftedRect(findDesignRect(buttonRects, buttonRectCount, "cancel"), dx)));
	const auto* cells = wide ? Gen::kRowCellsWide : Gen::kRowCellsCompact;
	const int cellCount = wide ? Gen::kRowCellWideCount : Gen::kRowCellCompactCount;
	int designCellW[2] = {50, 50};
	if (cellCount >= 2)
		for (int c = 0; c < 2; ++c)
			designCellW[c] = cells[c].w;
	conformTransferList(state._lstBases, state._window,
		generated->viewport.w, rowHit.h,
		designCellW, 2, -1,
		generated->rowHeight, generated->visibleRows,
		generated->scrollBarWidth, generated->minThumbHeight,
		state._game ? state._game->getMod() : nullptr, 1.0);
}

void CalypsoF12TransferBaseUi::configure(TransferBaseState& state)
{
	if (state._hdAdapter)
	{
		CalypsoHdUiOverlay::instance().clearAdapter(state._hdAdapter);
		delete state._hdAdapter;
		state._hdAdapter = nullptr;
		state._hdLayout = false;
	}
	if (!state._game || !state._game->getMod() || !state._game->getLanguage())
		CalypsoHdUiOverlay::instance().failHdRoute("F12 destination prerequisites are unavailable");
	if (!state._game->getMod()->isHdUiFamilyEnabled("F12"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;

	state._hdWideLayout = currentLayoutClass() == CalypsoLayoutClass::Wide;
	state._hdHarnessGeneration = Calypso::calypsoHarnessSession().generation;
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF12TransferBaseGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 destination generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	auto* adapter = new CalypsoF12TransferBaseUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
	if (calypsoHarnessHostUp(calypsoHarnessSession())) calypsoHdHarnessDomShow();
}

bool CalypsoF12TransferBaseUi::resize(TransferBaseState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = currentLayoutClass() == CalypsoLayoutClass::Wide;
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF12TransferBaseGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	return true;
}

void CalypsoF12TransferBaseUi::teardown(TransferBaseState& state)
{
	calypsoHdHarnessTeardownForTarget(&state, state._hdHarnessGeneration);
	delete state._hdAdapter;
	state._hdAdapter = nullptr;
}

CalypsoF12TransferItemsUi::~CalypsoF12TransferItemsUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF12TransferItemsUi::topState() const
{
	return _state;
}

bool CalypsoF12TransferItemsUi::suppressLogicalState() const
{
	// The category/search toolbar stays live native inputs (F11 precedent);
	// only the claimed chrome is suppressed, explicitly below.
	return false;
}

void CalypsoF12TransferItemsUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtQuantity);
	suppression.add(_state->_txtAmountTransfer);
	suppression.add(_state->_txtAmountDestination);
	suppression.add(_state->_lstItems);
	suppression.add(_state->_btnOk);
	suppression.add(_state->_btnCancel);
	suppression.add(_state->_cbxCategory);
	suppression.add(_state->_btnQuickSearch);
}

void CalypsoF12TransferItemsUi::collect(CalypsoHdFrameBuilder& builder) const
{
	namespace Gen = CalypsoF12TransferItemsGen;
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer generated layout is missing");

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
	model.listWidget = _state->_lstItems;
	model.titleText = Gen::kTitle;

	const auto* slots = wide ? Gen::kRowSlotsWide : Gen::kRowSlotsCompact;
	const int slotCount = wide ? Gen::kRowSlotWideCount : Gen::kRowSlotCompactCount;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));
	if (Gen::kColumnCount != 4)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer column contract drifted");
	for (int c = 0; c < Gen::kColumnCount; ++c)
		model.columnLabels.push_back(Gen::kColumns[c].label);
	const auto* headers = wide ? Gen::kColumnHeadersWide : Gen::kColumnHeadersCompact;
	const int headerCount = wide ? Gen::kColumnHeaderWideCount : Gen::kColumnHeaderCompactCount;
	if (headerCount != Gen::kColumnCount)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer header contract drifted");
	Text* headerWidgets[4] = {nullptr, _state->_txtQuantity, _state->_txtAmountTransfer, _state->_txtAmountDestination};
	for (int c = 0; c < headerCount; ++c)
	{
		model.columnHeaders.push_back(project(headers[c]));
		model.headerWidgets.push_back(c < 4 ? headerWidgets[c] : nullptr);
	}
	model.rowCells.resize(model.rowSlots.size());
	const auto* cells = wide ? Gen::kRowCellsWide : Gen::kRowCellsCompact;
	const int cellCount = wide ? Gen::kRowCellWideCount : Gen::kRowCellCompactCount;
	if (cellCount != slotCount * Gen::kColumnCount)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer cell contract drifted");
	for (int i = 0; i < cellCount; ++i)
		model.rowCells[i / Gen::kColumnCount].push_back(project(cells[i]));
	const auto* genSteppers = wide ? Gen::kRowSteppersWide : Gen::kRowSteppersCompact;
	const int genStepperCount = wide ? Gen::kRowStepperWideCount : Gen::kRowStepperCompactCount;
	if (genStepperCount != slotCount)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer stepper contract drifted");
	for (int i = 0; i < genStepperCount; ++i)
	{
		CalypsoScrollableCollectionStepper entry{};
		if (genSteppers[i].behaviorOwner == nullptr || genSteppers[i].behaviorOwner[0] == '\0')
			CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer stepper owner is missing");
		entry.owner = genSteppers[i].behaviorOwner;
		entry.decrement = project(genSteppers[i].decrement);
		entry.increment = project(genSteppers[i].increment);
		model.steppers.push_back(entry);
	}

	model.hasHeaderArt = generated->hasHeaderArt != 0;
	model.headerArtPath = Gen::kHeaderArtVfsPath;
	model.headerArtOpacity = std::max(1, std::min(100, (int)Gen::kHeaderArtOpacityPct)) / 100.0f;
	model.headerArtScrim = Gen::kHeaderArtScrim;

	if (_state->_lstItems)
	{
		const std::size_t total = _state->_lstItems->getTexts();
		for (std::size_t row = 0; row < total; ++row)
		{
			CalypsoScrollableCollectionRow entry{};
			for (int c = 0; c < Gen::kColumnCount; ++c)
				entry.values.push_back(_state->_lstItems->getCellText(row, c));
			model.rows.push_back(entry);
		}
		model.scrollOffset = _state->_lstItems->getScroll();
		const unsigned int selected = _state->_lstItems->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
		if (_state->_lstItems->isCalypsoHdSelectionList())
		{
			const SDL_Rect track = _state->_lstItems->getCalypsoHdTrackRect();
			const SDL_Rect thumb = _state->_lstItems->getCalypsoHdThumbRect();
			model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
			model.nativeTrack = {track.x, track.y, track.w, track.h};
			model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
			model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
		}
	}

	{
		const auto* controlRects = wide ? Gen::kControlRectsWide : Gen::kControlRectsCompact;
		const int controlRectCount = wide ? Gen::kControlRectWideCount : Gen::kControlRectCompactCount;
		CalypsoScrollableCollectionControl category{};
		category.id = "category-filter";
		category.label = "CATEGORY";
		const size_t categorySel = _state->_cbxCategory ? _state->_cbxCategory->getSelected() : 0;
		if (!_state->_cbxCategory || categorySel >= _state->_cats.size())
			CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer category state is missing");
		category.comboBox = _state->_cbxCategory;
		// Expanded select popup is runtime state: live translated options,
		// selection, and hover from the native behavior owner, painted over
		// the native popup list's exact input geometry.
		if (_state->_cbxCategory && _state->_cbxCategory->isPopupOpen())
		{
			const TextList* popup = _state->_cbxCategory->popupList();
			if (popup && popup->getTexts() > 0)
			{
				category.popupOpen = true;
				for (size_t i = 0; i < popup->getTexts(); ++i)
					category.popupOptions.push_back(popup->getCellText(i, 0));
				category.popupSelected = _state->_cbxCategory->getSelected();
				category.popupHovered = _state->_cbxCategory->getHoveredListIdx();
				category.popupScroll = popup->getScroll();
				category.popupVisibleRows = popup->getVisibleRows();
				category.popupRect = {popup->getX(), popup->getY(),
					popup->getWidth(), popup->getHeight()};
			}
		}
		category.value = _state->tr(_state->_cats[categorySel]);
		category.rect = findRect(controlRects, controlRectCount, "category-filter", project);
		if (category.rect.w <= 0 || category.rect.h <= 0)
			CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer control geometry is missing");
		category.widget = _state->_cbxCategory;
		model.controls.push_back(category);
		CalypsoScrollableCollectionControl query{};
		query.id = "quick-search";
		query.label = "SEARCH";
		if (!_state->_btnQuickSearch)
			CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer search state is missing");
		query.value = _state->_btnQuickSearch->getText();
		query.placeholder = "TYPE TO FILTER";
		query.rect = findRect(controlRects, controlRectCount, "quick-search", project);
		if (query.rect.w <= 0 || query.rect.h <= 0)
			CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer control geometry is missing");
		query.widget = _state->_btnQuickSearch;
		query.textEdit = _state->_btnQuickSearch;
		model.controls.push_back(query);
	}

	struct ButtonBinding { TextButton* widget; const char* id; };
	const ButtonBinding bindings[] = {{_state->_btnOk, "transfer"}, {_state->_btnCancel, "cancel"}};
	for (const auto& binding : bindings)
	{
		if (!binding.widget || !binding.widget->getVisible()) continue;
		const auto* generatedButton = findButton(Gen::kButtons, Gen::kButtonCount, binding.id);
		if (!generatedButton)
			CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer button contract drifted");
		CalypsoSmallConfirmationButton button{};
		button.widget = binding.widget;
		button.text = generatedButton->label;
		const auto* rects = wide ? Gen::kButtonRectsWide : Gen::kButtonRectsCompact;
		const int rectCount = wide ? Gen::kButtonRectWideCount : Gen::kButtonRectCompactCount;
		button.rect = findRect(rects, rectCount, binding.id, project);
		if (button.rect.w <= 0 || button.rect.h <= 0)
			CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer button geometry is missing");
		button.tone = std::string(generatedButton->tone) == "warning"
			? CalypsoActionTone::Destructive
			: (std::string(generatedButton->tone) == "primary"
				? CalypsoActionTone::Primary : CalypsoActionTone::Safe);
		button.restFill = generatedButton->fill;
		button.restBorder = generatedButton->border;
		button.textColor = generatedButton->text;
		model.buttons.push_back(button);
	}
	for (auto& button : model.buttons)
		button.peer = model.buttons.size() > 1
			? (button.widget == model.buttons.front().widget
				? model.buttons.back().widget : model.buttons.front().widget)
			: nullptr;

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

void CalypsoF12TransferItemsUi::applyGeneratedLayout(TransferItemsState& state, bool wide)
{
	namespace Gen = CalypsoF12TransferItemsGen;
	const auto* generated = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	const int dx = presentationShiftX(generated->window.w, generated->window.x, wide);
	applyRect(state._window, shiftedRect(generated->window, dx));
	applyRect(state._txtTitle, shiftedRect(generated->title, dx));
	const auto& rowHit = wide ? Gen::kRowHitWide : Gen::kRowHitCompact;
	// Native input starts at the first painted row but spans the full
	// viewport width: the reserved right rail carries the scrollbar track,
	// so the track can never overlap a 44px stepper target.
	applyRect(state._lstItems, {rowHit.x + dx, rowHit.y, generated->viewport.w, rowHit.h});
	const auto* buttonRects = wide ? Gen::kButtonRectsWide : Gen::kButtonRectsCompact;
	const int buttonRectCount = wide ? Gen::kButtonRectWideCount : Gen::kButtonRectCompactCount;
	applyRect(state._btnOk, touchRect(shiftedRect(findDesignRect(buttonRects, buttonRectCount, "transfer"), dx)));
	applyRect(state._btnCancel, touchRect(shiftedRect(findDesignRect(buttonRects, buttonRectCount, "cancel"), dx)));
	const auto* controlRects = wide ? Gen::kControlRectsWide : Gen::kControlRectsCompact;
	const int controlRectCount = wide ? Gen::kControlRectWideCount : Gen::kControlRectCompactCount;
	applyRect(state._cbxCategory, shiftedRect(findDesignRect(controlRects, controlRectCount, "category-filter"), dx));
	applyRect(state._btnQuickSearch, shiftedRect(findDesignRect(controlRects, controlRectCount, "quick-search"), dx));
	const auto* cells = wide ? Gen::kRowCellsWide : Gen::kRowCellsCompact;
	const int cellCount = wide ? Gen::kRowCellWideCount : Gen::kRowCellCompactCount;
	int designCellW[4] = {50, 50, 50, 50};
	if (cellCount >= 4)
		for (int c = 0; c < 4; ++c)
			designCellW[c] = cells[c].w;
	const auto* headers = wide ? Gen::kColumnHeadersWide : Gen::kColumnHeadersCompact;
	const int headerCount = wide ? Gen::kColumnHeaderWideCount : Gen::kColumnHeaderCompactCount;
	int designArrowX = generated->viewport.x;
	if (headerCount >= 4)
		designArrowX = (headers[1].x - generated->viewport.x) + 22;
	conformTransferList(state._lstItems, state._window,
		generated->viewport.w, rowHit.h,
		designCellW, 4, designArrowX,
		generated->rowHeight, generated->visibleRows,
		generated->scrollBarWidth, generated->minThumbHeight,
		state._game ? state._game->getMod() : nullptr, 1.0);
	// Bind the painted stepper affordances to the existing native arrow
	// handlers: the left button keeps increase, so it sits on the increment
	// target; the right button keeps decrease, so it sits on decrement.
	const int stepperCount = wide ? Gen::kRowStepperWideCount : Gen::kRowStepperCompactCount;
	if (stepperCount <= 0)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer stepper contract is missing");
	const auto& stepper0 = (wide ? Gen::kRowSteppersWide : Gen::kRowSteppersCompact)[0];
	if (state._lstItems)
		state._lstItems->setCalypsoHdArrowTargets(
			stepper0.increment.x + dx, stepper0.decrement.x + dx,
			stepper0.decrement.w, stepper0.decrement.h);
	// Recreate rows after conformance so arrow buttons are born with the HD
	// stepper geometry instead of the legacy arrow column.
	state.updateList();
}

void CalypsoF12TransferItemsUi::configure(TransferItemsState& state)
{
	if (state._hdAdapter)
	{
		CalypsoHdUiOverlay::instance().clearAdapter(state._hdAdapter);
		delete state._hdAdapter;
		state._hdAdapter = nullptr;
		state._hdLayout = false;
	}
	if (!state._game || !state._game->getMod() || !state._game->getLanguage())
		CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer prerequisites are unavailable");
	if (!state._game->getMod()->isHdUiFamilyEnabled("F12"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;

	state._hdWideLayout = currentLayoutClass() == CalypsoLayoutClass::Wide;
	state._hdHarnessGeneration = Calypso::calypsoHarnessSession().generation;
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF12TransferItemsGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F12 transfer generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	auto* adapter = new CalypsoF12TransferItemsUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
	// The approved HD control is always painted; a hidden native owner cannot
	// take focus/input, so keep it visible while this adapter is active. Its
	// pixels stay suppressed; presentation comes only from the HD control.
	if (state._btnQuickSearch) state._btnQuickSearch->setVisible(true);
	if (calypsoHarnessHostUp(calypsoHarnessSession())) calypsoHdHarnessDomShow();
}

bool CalypsoF12TransferItemsUi::resize(TransferItemsState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = currentLayoutClass() == CalypsoLayoutClass::Wide;
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF12TransferItemsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	return true;
}

void CalypsoF12TransferItemsUi::teardown(TransferItemsState& state)
{
	calypsoHdHarnessTeardownForTarget(&state, state._hdHarnessGeneration);
	delete state._hdAdapter;
	state._hdAdapter = nullptr;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
