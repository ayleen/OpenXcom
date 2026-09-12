#ifdef __EMSCRIPTEN__
#include "CalypsoF15TechTreeUi.h"

#include "CalypsoHdFontSource.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdOperationsLayout.h"
#include "CalypsoHdOperationsRenderer.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoViewportRuntime.h"
#include "Generated/CalypsoF15TechTree.generated.h"
#include "../Basescape/TechTreeViewerState.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"
#include <algorithm>
#include <cmath>

namespace OpenXcom { namespace Calypso {
namespace {

template <typename R>
CalypsoHdOperationsRect projectRect(const R &r, int wx, int wy, double sx, double sy,
	int windowX, int windowY)
{
	return {
		wx + static_cast<int>(std::lround((r.x - windowX) * sx)),
		wy + static_cast<int>(std::lround((r.y - windowY) * sy)),
		std::max(1, static_cast<int>(std::lround(r.w * sx))),
		std::max(1, static_cast<int>(std::lround(r.h * sy)))};
}

void setWindowRect(Window *window, const CalypsoHdOperationsRect &rect)
{
	const auto projected = calypsoHdOperationsProjectForCurrentPresentation(
		{rect.x, rect.y, rect.w, rect.h}, rect.w, rect.h);
	if (window->getX() != projected.x) window->setX(projected.x);
	if (window->getY() != projected.y) window->setY(projected.y);
	if (window->getWidth() != projected.w) window->setWidth(projected.w);
	if (window->getHeight() != projected.h) window->setHeight(projected.h);
}

void setFonts(CalypsoHdOperationsModel &model, const Mod *mod)
{
	model.readiness.contractReady = true;
	model.readiness.uploadsReady = true;
	model.readiness.retryable = true;
	model.readiness.fontsReady =
		mod != nullptr
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_SB", model.headingFont)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_R", model.bodyFont)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_PLEX_R", model.monoFont);
}

CalypsoHdOperationsAction action(const std::string &id, const std::string &label,
	const CalypsoHdOperationsRect &rect, const void *widget, bool visible = true,
	const std::string &tone = "normal")
{
	CalypsoHdOperationsAction out;
	out.id = id;
	out.label = label;
	out.component = "management-action-group";
	out.slotRole = "action";
	out.coordinateSpace = "logical";
	out.tone = tone;
	out.visible = rect;
	out.hit = rect;
	out.widget = widget;
	out.state.visible = visible;
	return out;
}

/// Projects the generated track itself into the native descriptor: the data
/// viewport height is exactly the painted track height (re-review P2).
template <typename R, typename Collection>
void configureHdList(TextList &list, const R &parent, const R &rowSlot1,
	const R &scrollTrack, const Collection &generated, double sx, double sy)
{
	const auto descriptor = Calypso::calypsoSelectionListDescriptorFor(
		static_cast<int>(std::lround(parent.y * sy)),
		static_cast<int>(std::lround(scrollTrack.y * sy)),
		static_cast<int>(std::lround(scrollTrack.w * sx)),
		static_cast<int>(std::lround(scrollTrack.h * sy)),
		static_cast<int>(std::lround(rowSlot1.h * sy)),
		static_cast<std::size_t>(generated.rowSlotCount),
		static_cast<int>(std::lround(44 * sy)));
	list.configureCalypsoHdSelectionList(
		descriptor.scrollBarWidth, descriptor.minThumbHeight, descriptor.rowStride,
		descriptor.rowOriginY, descriptor.dataViewportH, descriptor.visibleRows);
}

} // namespace

CalypsoF15TechTreeUi::CalypsoF15TechTreeUi(TechTreeViewerState *state)
	: _state(state), _chrome(new CalypsoHdOperationsChrome(*state))
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { syncGeometry(); return buildModel(); });
}

CalypsoF15TechTreeUi::~CalypsoF15TechTreeUi()
{
	delete _renderer;
	delete _chrome;
}

void CalypsoF15TechTreeUi::configure(TechTreeViewerState &state)
{
	if (state._hdAdapter != nullptr) return;
	if (!calypsoHdOperationsRouteEnabled(state._game, "F15"))
	{
		state._hdLayout = false;
		return;
	}
	// Responsive class from the canonical logical viewport (re-review P1).
	const auto &viewport = calypsoViewportRuntime().current();
	const auto layoutClass = classifyCalypsoHdOperationsLayout(
		std::max(1, viewport.logicalWidth), std::max(1, viewport.logicalHeight));
	if (layoutClass == CalypsoHdOperationsLayoutClass::Unsupported)
	{
		CalypsoHdUiOverlay::instance().failHdRoute(
			"Tech Tree HD viewport is below the 740x360 minimum");
	}
	state._hdLayout = true;
	state._hdWideLayout = layoutClass == CalypsoHdOperationsLayoutClass::Wide;
	auto *adapter = new CalypsoF15TechTreeUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter->_renderer);
	adapter->refresh();
	calypsoHdOperationsPublishHarnessVisibility();
}

bool CalypsoF15TechTreeUi::resize(TechTreeViewerState &state)
{
	if (!state._hdLayout || !state._hdAdapter) return false;
	const auto &viewport = calypsoViewportRuntime().current();
	const auto layoutClass = classifyCalypsoHdOperationsLayout(
		std::max(1, viewport.logicalWidth), std::max(1, viewport.logicalHeight));
	if (layoutClass == CalypsoHdOperationsLayoutClass::Unsupported)
	{
		CalypsoHdUiOverlay::instance().failHdRoute(
			"Tech Tree HD viewport is below the 740x360 minimum");
	}
	state._hdWideLayout = layoutClass == CalypsoHdOperationsLayoutClass::Wide;
	state._hdAdapter->refresh();
	return true;
}

void CalypsoF15TechTreeUi::refresh()
{
	if (!_renderer) return;
	syncGeometry();
	_renderer->setModel(buildModel());
}

void CalypsoF15TechTreeUi::syncGeometry()
{
	applyGeometry();
	if (_chrome) _chrome->applyGeometry();
}

void CalypsoF15TechTreeUi::applyGeometry()
{
	if (!_state || !_state->_window) return;
	const bool wide = _state->_hdWideLayout;
	const auto *g = CalypsoF15TechTreeGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	setWindowRect(_state->_window, {g->window.x, g->window.y, g->window.w, g->window.h});
	const int wx = _state->_window->getX(), wy = _state->_window->getY();
	const double sx = static_cast<double>(_state->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_state->_window->getHeight()) / g->window.h;
	auto place = [&](Surface *surface, const auto &rect)
	{
		if (!surface) return;
		const auto p = projectRect(rect, wx, wy, sx, sy, g->window.x, g->window.y);
		if (surface->getX() != p.x) surface->setX(p.x);
		if (surface->getY() != p.y) surface->setY(p.y);
		if (surface->getWidth() != p.w) surface->setWidth(p.w);
		if (surface->getHeight() != p.h) surface->setHeight(p.h);
	};
	place(_state->_txtTitle, g->title);
	place(_state->_txtSelectedTopic, g->region_left_label);
	place(_state->_txtProgress, g->region_right_label);
	place(_state->_txtCostIndicator, g->region_left_content);
	place(_state->_lstLeft, g->region_left_collection);
	place(_state->_lstRight, g->region_right_collection);
	place(_state->_lstFull, g->region_left_collection);
	place(_state->_btnNew, g->action_select_topic);
	place(_state->_btnOk, g->action_ok);
	const auto &collections = wide
		? CalypsoF15TechTreeGen::kCollectionsWide
		: CalypsoF15TechTreeGen::kCollectionsCompact;
	const auto &left = collections[0];
	const auto &right = collections[1];
	configureHdList(*_state->_lstLeft, g->region_left_collection,
		g->region_left_collection_row_slot_1, g->region_left_collection_scroll_track,
		left, sx, sy);
	configureHdList(*_state->_lstRight, g->region_right_collection,
		g->region_right_collection_row_slot_1, g->region_right_collection_scroll_track,
		right, sx, sy);
	configureHdList(*_state->_lstFull, g->region_left_collection,
		g->region_left_collection_row_slot_1, g->region_left_collection_scroll_track,
		left, sx, sy);
}

CalypsoHdOperationsModel CalypsoF15TechTreeUi::buildModel() const
{
	CalypsoHdOperationsModel model;
	if (!_state || !_state->_window || !_state->_game || !_state->_lstLeft
		|| !_state->_lstRight || !_state->_lstFull || !_state->_btnOk
		|| !_state->_btnNew || !_state->_game->getMod()) return model;
	const auto tr = [this](const std::string &key) { return _state->tr(key); };
	const bool wide = _state->_hdWideLayout;
	const auto *g = CalypsoF15TechTreeGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	auto p = [&](const auto &r) { return CalypsoHdOperationsRect{r.x, r.y, r.w, r.h}; };
	model.archetype = CalypsoHdOperationsArchetype::WideDetail;
	model.familyId = CalypsoF15TechTreeGen::kFamilyId;
	model.visualShell = CalypsoF15TechTreeGen::kVisualShell;
	model.headerArtId = CalypsoF15TechTreeGen::kHeaderArt;
	model.title = _state->_txtTitle->getText();
	model.baseCaption = "BASES";
	setFonts(model, _state->_game->getMod());
	model.presentation = CalypsoF15TechTreeGen::kPresentationProfile;
	model.profileId = CalypsoF15TechTreeGen::kProfileId;
	model.profileVersion = CalypsoF15TechTreeGen::kProfileVersion;
	model.provenance = CalypsoF15TechTreeGen::kProvenanceTemplate;
	model.readiness.contractReady = true;
	model.ownerState = _state;
	model.geometry.designWidth = g->designWidth;
	model.geometry.designHeight = g->designHeight;
	model.geometry.window = p(g->window);
	model.geometry.status = p(g->status);
	model.geometry.title = p(g->title);
	model.geometry.screenHeader = p(g->status);
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.controlBar = p(g->controlBar);
	model.geometry.footer = p(g->footer);
	model.suppressedWidgets = {
		_state->_window, _state->_txtTitle, _state->_txtSelectedTopic,
		_state->_txtProgress, _state->_txtCostIndicator,
		_state->_lstLeft, _state->_lstRight, _state->_lstFull,
		_state->_btnNew, _state->_btnOk};
	calypsoHdOperationsApplyGeneratedStyle(model, CalypsoF15TechTreeGen::kProfileStyle);
	calypsoHdOperationsApplyGeneratedTypography(model,
		wide ? CalypsoF15TechTreeGen::kTypographyWide
			: CalypsoF15TechTreeGen::kTypographyCompact);

	// The two synchronized native lists map onto the two typed collections;
	// the items mode presents the full-width list alone. Row text comes from
	// the native snapshot, row kind from the owner's parallel metadata —
	// nothing is re-derived and hidden topics stay "***".
	struct ListSource
	{
		TextList *list;
		const std::vector<TechTreeViewerState::RowMeta> *meta;
		const CalypsoF15TechTreeGen::CalypsoF15TechTreeGenCollectionLayout *generated;
		CalypsoHdOperationsRegion *region;
		const CalypsoF15TechTreeGen::CalypsoF15TechTreeGenRect *labelRect;
		const CalypsoF15TechTreeGen::CalypsoF15TechTreeGenRect *viewport;
		const CalypsoF15TechTreeGen::CalypsoF15TechTreeGenRect *track;
		const CalypsoF15TechTreeGen::CalypsoF15TechTreeGenRect *rowSlot;
	};
	CalypsoHdOperationsRegion leftRegion;
	CalypsoHdOperationsRegion rightRegion;
	leftRegion.id = "left";
	rightRegion.id = "right";
	const bool itemsMode = _state->_selectedFlag == TTV_ITEMS;
	ListSource sources[2] = {
		{ itemsMode ? _state->_lstFull : _state->_lstLeft,
		  itemsMode ? &_state->calypsoFullRows() : &_state->calypsoLeftRows(),
		  wide ? &CalypsoF15TechTreeGen::kCollectionsWide[0]
			: &CalypsoF15TechTreeGen::kCollectionsCompact[0],
		  &leftRegion, &g->region_left_label, &g->region_left_collection,
		  &g->region_left_collection_scroll_track,
		  &g->region_left_collection_row_slot_1 },
		{ itemsMode ? nullptr : _state->_lstRight,
		  &_state->calypsoRightRows(),
		  wide ? &CalypsoF15TechTreeGen::kCollectionsWide[1]
			: &CalypsoF15TechTreeGen::kCollectionsCompact[1],
		  &rightRegion, &g->region_right_label, &g->region_right_collection,
		  &g->region_right_collection_scroll_track,
		  &g->region_right_collection_row_slot_1 },
	};
	for (auto &source : sources)
	{
		CalypsoHdOperationsRegion &region = *source.region;
		region.kind = CalypsoHdOperationsRegionKind::Collection;
		region.label = tr("STR_TOPIC");
		region.rect = p(*source.viewport);
		region.labelRect = p(*source.labelRect);
		region.collection.viewport = p(*source.viewport);
		region.collection.scrollTrack = p(*source.track);
		region.collection.scrollThumb = p(*source.track);
		region.collection.emptyTitle = tr("STR_NO_DEPENDENCIES");
		region.collection.emptyBody = tr("STR_NONE");
		region.collection.columns.push_back({
			"topic", tr("STR_TOPIC"), p(source.generated->columns[0].rect), {},
			source.generated->columns[0].contentRole});
		region.collection.rowHeight = p(*source.rowSlot).h;
		region.collection.visibleRows = static_cast<std::size_t>(source.generated->rowSlotCount);
		for (int i = 0; i < source.generated->rowSlotCount; ++i)
			region.collection.rowSlots.push_back(p(source.generated->rowSlots[i].rect));
		if (source.list == nullptr)
		{
			region.state.visible = false;
			model.regions.push_back(std::move(region));
			continue;
		}
		const auto snapshot = source.list->getCellTextsSnapshot();
		const auto &meta = *source.meta;
		const std::size_t nativeSelected =
			static_cast<std::size_t>(source.list->getSelectedRow());
		const std::size_t nativeOffset = source.list->getScroll();
		for (std::size_t i = 0; i < snapshot.size(); ++i)
		{
			const std::string text = snapshot[i].empty() || !snapshot[i][0]
				? std::string() : snapshot[i][0]->getText();
			const bool structural = i < meta.size() && meta[i].structural;
			const bool hidden = i < meta.size() && meta[i].hidden;
			CalypsoHdOperationsRow row;
			row.id = (source.list == _state->_lstRight ? "right-" : "left-")
				+ std::to_string(i);
			if (!text.empty())
			{
				row.values.push_back(text);
				CalypsoHdOperationsCell cell;
				cell.value = text;
				cell.contentRole = "name";
				cell.state.disabled = structural || hidden;
				row.cells.push_back(std::move(cell));
			}
			const std::size_t slot = source.generated->rowSlotCount == 0 ? 0
				: (i >= nativeOffset ? i - nativeOffset : 0)
					% static_cast<std::size_t>(source.generated->rowSlotCount);
			row.rect = region.collection.rowSlots.empty()
				? region.collection.viewport : region.collection.rowSlots[slot];
			row.state.disabled = structural || hidden;
			row.state.selected = i == nativeSelected;
			row.widget = source.list;
			region.collection.rows.push_back(std::move(row));
		}
		region.collection.selectedIndex = nativeSelected;
		region.collection.scrollOffset = nativeOffset;
		region.collection.count = snapshot.size();
		region.collection.scroll = {
			nativeOffset, snapshot.size(),
			static_cast<std::size_t>(source.generated->rowSlotCount),
			region.collection.viewport, region.collection.scrollTrack,
			region.collection.scrollThumb};
		model.regions.push_back(std::move(region));
	}
	model.geometry.footerActions = {p(g->action_select_topic), p(g->action_ok)};
	model.footerActions.push_back(action("select-topic",
		_state->_btnNew->getText(), p(g->action_select_topic), _state->_btnNew,
		_state->_btnNew->getVisible(), "safe"));
	model.footerActions.push_back(action("ok", _state->_btnOk->getText(),
		p(g->action_ok), _state->_btnOk, true, "primary"));
	return model;
}

} } // namespace OpenXcom::Calypso

#endif // __EMSCRIPTEN__
