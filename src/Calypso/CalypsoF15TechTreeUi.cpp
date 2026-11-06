#ifdef __EMSCRIPTEN__
#include "CalypsoF15TechTreeUi.h"

#include "CalypsoHdFontSource.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdOperationsFluid.h"
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
using FluidAnchor = CalypsoHdOperationsFluidPolicy::Anchor;
using FluidVRole = CalypsoHdOperationsFluidPolicy::VerticalRole;


template <typename R>
void setSurfaceRect(Surface *surface, const R &rect)
{
	if (!surface) return;
	if (surface->getX() != rect.x) surface->setX(rect.x);
	if (surface->getY() != rect.y) surface->setY(rect.y);
	if (surface->getWidth() != rect.w) surface->setWidth(rect.w);
	if (surface->getHeight() != rect.h) surface->setHeight(rect.h);
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

/// Resolves CSS capacity first, then projects the resulting viewport/track
/// metrics once into the native descriptor. Mixing projected engine coords
/// back into the CSS policy applies the presentation scale twice.
void configureHdList(TextList &list, const CalypsoHdOperationsFluidFrame &fluid,
	const CalypsoF15TechTreeGen::CalypsoF15TechTreeGenRect &parent,
	const CalypsoF15TechTreeGen::CalypsoF15TechTreeGenRect &rowSlot1,
	const CalypsoF15TechTreeGen::CalypsoF15TechTreeGenRect &scrollTrack,
	const CalypsoF15TechTreeGen::CalypsoF15TechTreeGenCollectionLayout &generated,
	FluidAnchor parentAnchor = FluidAnchor::Geometry)
{
	(void)generated;
	const auto descriptor = fluid.selectionListGeometry(
		{parent.x, parent.y, parent.w, parent.h},
		{rowSlot1.x, rowSlot1.y, rowSlot1.w, rowSlot1.h},
		{scrollTrack.x, scrollTrack.y, scrollTrack.w, scrollTrack.h},
		FluidVRole::Stretch, parentAnchor);
	list.configureCalypsoHdSelectionList(
		descriptor.scrollBarWidth, descriptor.minThumbHeight,
		descriptor.rowStride, descriptor.rowOriginY,
		descriptor.dataViewportH, descriptor.visibleRows);
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
	const auto effective = calypsoHdOperationsEffectiveViewport();
	const auto layoutClass = classifyCalypsoHdOperationsLayout(
		effective.cssWidth, std::max(1, viewport.logicalHeight));
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
	const auto effective = calypsoHdOperationsEffectiveViewport();
	const auto layoutClass = classifyCalypsoHdOperationsLayout(
		effective.cssWidth, std::max(1, viewport.logicalHeight));
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
	const auto fluid = CalypsoHdOperationsFluidFrame::forReference(
		g->window.w, g->window.h, g->footer.x, g->footer.w);
	auto place = [&](Surface *surface, const auto &rect,
		FluidVRole vrole = FluidVRole::Fixed,
		FluidAnchor anchor = FluidAnchor::Geometry)
	{
		setSurfaceRect(surface, fluid.resolve(
			CalypsoHdOperationsRectf{rect.x, rect.y, rect.w, rect.h}, vrole, anchor));
	};
	place(_state->_txtTitle, g->title);
	place(_state->_txtSelectedTopic, g->region_left_label);
	place(_state->_txtProgress, g->region_right_label,
		FluidVRole::Fixed, FluidAnchor::Right);
	place(_state->_txtCostIndicator, g->region_left_content);
	place(_state->_lstLeft, g->region_left_collection, FluidVRole::Stretch);
	place(_state->_lstRight, g->region_right_collection,
		FluidVRole::Stretch, FluidAnchor::Right);
	place(_state->_lstFull, g->region_left_collection, FluidVRole::Stretch);
	place(_state->_btnNew, g->action_select_topic, FluidVRole::BottomShift);
	place(_state->_btnOk, g->action_ok, FluidVRole::BottomShift);
	const auto &collections = wide
		? CalypsoF15TechTreeGen::kCollectionsWide
		: CalypsoF15TechTreeGen::kCollectionsCompact;
	const auto &left = collections[0];
	const auto &right = collections[1];
	configureHdList(*_state->_lstLeft, fluid, g->region_left_collection,
		g->region_left_collection_row_slot_1, g->region_left_collection_scroll_track, left);
	configureHdList(*_state->_lstRight, fluid, g->region_right_collection,
		g->region_right_collection_row_slot_1, g->region_right_collection_scroll_track,
		right, FluidAnchor::Right);
	configureHdList(*_state->_lstFull, fluid, g->region_left_collection,
		g->region_left_collection_row_slot_1, g->region_left_collection_scroll_track, left);
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
	const auto fluid = CalypsoHdOperationsFluidFrame::forReference(
		g->window.w, g->window.h, g->footer.x, g->footer.w);
	auto p = [&](const auto &r, FluidVRole vr = FluidVRole::Fixed,
		FluidAnchor ha = FluidAnchor::Geometry) {
		const auto css = fluid.cssY(fluid.cssX(
			CalypsoHdOperationsRectf{r.x, r.y, r.w, r.h}, ha), vr);
		return CalypsoHdOperationsRect{css.x, css.y, css.w, css.h};
	};
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
	model.geometry.designWidth = fluid.width();
	model.geometry.designHeight = fluid.height();
	model.geometry.window = {0, 0, fluid.width(), fluid.height()};
	model.geometry.status = p(g->status);
	model.geometry.title = p(g->title);
	model.geometry.screenHeader = p(g->status);
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.controlBar = p(g->controlBar);
	model.geometry.footer = p(g->footer, FluidVRole::BottomShift);
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
	// STR_TOPIC is a format string in the native locale; publish the same
	// resolved empty-argument label used by TechTreeViewerState instead of
	// leaking its placeholder into the HD column headers.
	const std::string topicLabels[] = {tr("STR_REQUIRES"), tr("STR_UNLOCKS")};
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
		region.label = topicLabels[source.region == &rightRegion ? 1 : 0];
		const int headerHeight = source.rowSlot->y - source.viewport->y;
		const bool rightGroup = source.region == &rightRegion;
		const auto groupAnchor = rightGroup ? FluidAnchor::Right : FluidAnchor::Geometry;
		const auto viewport = p(*source.viewport, FluidVRole::Stretch, groupAnchor);
		const auto track = p(CalypsoHdOperationsRectf{
			source.track->x, source.viewport->y + headerHeight, source.track->w, 0},
			FluidVRole::Fixed, groupAnchor);
		calypsoHdOperationsResolveCollectionGeometry(region.collection,
			viewport, p(*source.rowSlot, FluidVRole::Fixed, groupAnchor), track, headerHeight,
			source.viewport->w);
		region.rect = viewport;
		region.labelRect = p(*source.labelRect, FluidVRole::Fixed, groupAnchor);
		region.collection.emptyTitle = tr("STR_NO_DEPENDENCIES");
		region.collection.emptyBody = tr("STR_NONE");
		region.collection.columns.push_back({
			"topic", region.label, p(source.generated->columns[0].rect,
				FluidVRole::Fixed, groupAnchor), {},
			source.generated->columns[0].contentRole});
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
			const std::size_t slot = region.collection.visibleRows == 0 ? 0
				: (i >= nativeOffset ? i - nativeOffset : 0)
					% region.collection.visibleRows;
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
			region.collection.visibleRows,
			region.collection.viewport, region.collection.scrollTrack,
			region.collection.scrollThumb};
		model.regions.push_back(std::move(region));
	}
	// Keep the native topic/type, progress and cost context visible in a
	// generated summary region.  These values are owned by the state and are
	// refreshed after every dependency navigation; the HD layer only presents
	// the localized strings and never reconstructs them from rule data.
	CalypsoHdOperationsRegion summary;
	summary.id = "summary";
	summary.kind = CalypsoHdOperationsRegionKind::Fields;
	summary.label = tr("STR_RESEARCH_PROGRESS").arg("");
	const auto summaryRole = wide ? FluidVRole::BottomShift : FluidVRole::Fixed;
	const auto summaryAnchor = wide ? FluidAnchor::Geometry : FluidAnchor::Right;
	auto summaryRect = [&](const auto& rect) {
		return p(rect, summaryRole, summaryAnchor);
	};
	summary.rect = summaryRect(g->region_summary);
	summary.labelRect = summaryRect(g->region_summary_label);
	if (_state->_txtSelectedTopic->getVisible())
		summary.fields.push_back({"topic", tr("STR_TOPIC").arg(""),
		_state->_txtSelectedTopic->getText(), summaryRect(g->region_summary_field_topic), {}});
	if (_state->_txtProgress->getVisible())
		summary.fields.push_back({"progress", tr("STR_PROGRESS"),
		_state->_txtProgress->getText(), summaryRect(g->region_summary_field_progress), {}});
	if (_state->_txtCostIndicator->getVisible()
		&& !_state->_txtCostIndicator->getText().empty())
		summary.fields.push_back({"cost", tr("STR_COST_UC"),
		_state->_txtCostIndicator->getText(), summaryRect(g->region_summary_field_cost), {}});
	model.regions.push_back(std::move(summary));
	model.geometry.footerActions = {
		p(g->action_select_topic, FluidVRole::BottomShift),
		p(g->action_ok, FluidVRole::BottomShift)};
	model.footerActions.push_back(action("select-topic",
		_state->_btnNew->getText(), p(g->action_select_topic, FluidVRole::BottomShift), _state->_btnNew,
		_state->_btnNew->getVisible(), "safe"));
	model.footerActions.push_back(action("ok", _state->_btnOk->getText(),
		p(g->action_ok, FluidVRole::BottomShift), _state->_btnOk, true, "primary"));
	// Nested N05 owners still use the real shared strategic rail. Populate it
	// from the same native TextButton owners used by the viewer; omitting this
	// made the rail paint but left the read-only snapshot with no actionable
	// navigation records.
	_chrome->populateModel(model);
	return model;
}

} } // namespace OpenXcom::Calypso

#endif // __EMSCRIPTEN__
