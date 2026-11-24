#ifdef __EMSCRIPTEN__
#include "../Geoscape/InterceptState.h"
#include "../Geoscape/ConfirmDestinationState.h"
#include "Generated/CalypsoF18CourseConfirm.generated.h"
#include "../Interface/ToggleTextButton.h"
#include "../Mod/RuleCraft.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/Waypoint.h"
#include "CalypsoHdOperationsBindings.h"
#include "CalypsoHdOperationsFluid.h"
#include "CalypsoHdOperationsRenderer.h"
#include "CalypsoHdOperationsLayout.h"
#include "CalypsoHdUiOverlay.h"
#include "Generated/CalypsoF18InterceptFleet.generated.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Target.h"

namespace OpenXcom {
using namespace Calypso;
namespace G = CalypsoF18InterceptFleetGen;
namespace {
using V = CalypsoHdOperationsFluidPolicy::VerticalRole;
using A = CalypsoHdOperationsFluidPolicy::Anchor;
template<class R> CalypsoHdOperationsRectf rect(R r) { return {r.x,r.y,r.w,r.h}; }
void moveOwner(Surface &owner, CalypsoHdOperationsRect r)
{
	if (owner.getX() != r.x) owner.setX(r.x);
	if (owner.getY() != r.y) owner.setY(r.y);
	if (owner.getWidth() != r.w) owner.setWidth(r.w);
	if (owner.getHeight() != r.h) owner.setHeight(r.h);
}
template<class Layout, class Paint>
void placeFrame(CalypsoHdOperationsModel &model, const Layout &g,
	const CalypsoHdOperationsFluidFrame &frame, Paint paint)
{
	auto &geo = model.geometry;
	geo.designWidth = frame.width(); geo.designHeight = frame.height();
	geo.window = {0,0,frame.width(),frame.height()};
	geo.screenHeader = paint(g.screenHeader); geo.headerArt = paint(g.headerArt);
	geo.title = paint(g.title); geo.summaryBar = paint(g.summaryBar);
	geo.toolbarBar = paint(g.toolbarBar); geo.footer = paint(g.footer,V::BottomShift);
}

template<class Layout, class Paint>
void placeTable(CalypsoHdOperationsModel &model, const Layout &g, Paint paint)
{
	auto &geo = model.geometry;
	auto &list = model.collection;
	const auto viewport = paint(g.collectionViewport,V::Stretch);
	const auto first = paint(g.collection_row_slot_1);
	calypsoHdOperationsResolveCollectionGeometry(list,viewport,first,paint(g.collection_scroll_track),
		first.y - viewport.y,g.collectionViewport.w);
	geo.collectionViewport = viewport;
	geo.collectionRows = list.rowSlots;
	geo.collectionScrollTrack = list.scrollTrack;
	geo.collectionScrollThumb = list.scrollThumb;
}
}
CalypsoHdOperationsModel InterceptState::fleetModel()
{
	using namespace OperationsBindings;
	const auto v = calypsoHdOperationsEffectiveViewport();
	const auto mode = classifyCalypsoHdOperationsLayout(v.cssWidth, v.cssHeight);
	if (mode == CalypsoHdOperationsLayoutClass::Unsupported)
		CalypsoHdUiOverlay::instance().failHdRoute("Fleet dispatch viewport is unavailable");
	const bool wide = mode == CalypsoHdOperationsLayoutClass::Wide;
	const auto &g = G::kLayouts[wide ? 0 : 1];
	const auto &table = wide ? G::kCollectionsWide[0] : G::kCollectionsCompact[0];
	const auto frame = CalypsoHdOperationsFluidFrame::forReference(g.designWidth,g.designHeight,g.footer.x,g.footer.w);
	auto paint = [&](auto r, V role=V::Fixed, A anchor=A::Geometry) {
		const auto css = frame.cssY(frame.cssX(rect(r),anchor),role);
		return CalypsoHdOperationsRect{css.x,css.y,css.w,css.h};
	};
	const auto anchor = wide ? A::Right : A::Geometry;
	CalypsoHdOperationsModel m;
	m.ownerState = this;
	m.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	m.familyId = G::kFamilyId;
	m.suppressedWidgets = {_window, _txtTitle, _txtCraft, _txtStatus, _txtBase, _txtWeapons, _lstCrafts};
	m.visualShell = G::kVisualShell;
	m.headerArtId = G::kHeaderArt;
	m.title = tr("STR_CAL_WING_ASSEMBLY");
	m.sectionLabel = tr("STR_INTERCEPT");
	m.baseCaption = tr("STR_BASE");
	m.baseName = _base ? _base->getName() : std::string(tr("STR_CAL_FLEET_MAP"));
	finish(m,_game->getMod(),G::kPresentationProfile,G::kProfileId,G::kProfileVersion,
		G::kProvenanceTemplate,G::kProfileStyle,G::kTypographyWide,G::kTypographyCompact,wide);
	auto &geo = m.geometry;
	placeFrame(m, g, frame, paint);
	Craft *craft = _picked < _crafts.size() ? _crafts[_picked] : nullptr;
	const bool inWing = craft && std::find(_selCrafts.begin(),_selCrafts.end(),craft) != _selCrafts.end();
	const auto wingCount = _selCrafts.size() + (canDispatch(craft) && !inWing ? 1 : 0);
	auto *language = _game->getLanguage();
	m.summaryFields = {
		summary("fleet",tr("STR_CAL_BERTHS"),std::to_string(_crafts.size()),paint(g.summary_fleet),nullptr),
		summary("selected",tr("STR_CAL_WING_SELECTED"),std::to_string(wingCount) + " / 4",paint(g.summary_selected),nullptr),
		summary("target",tr("STR_CAL_FLEET_TARGET"),_target ? _target->getName(language) : std::string(tr("STR_NONE")),paint(g.summary_target),nullptr)};
	auto &list = m.collection;
	placeTable(m, g, paint);
	const auto viewport = geo.collectionViewport;
	const auto first = paint(g.collection_row_slot_1);
	moveOwner(*_lstCrafts,frame.resolve(rect(g.collectionViewport),V::Stretch));
	const auto hit = frame.selectionListGeometry(rect(g.collectionViewport),rect(g.collection_row_slot_1),rect(g.collection_scroll_track));
	auto edges = frame.projectChooserRowBoundaries(rect(viewport),first.y-viewport.y,first.h,hit.visibleRows);
	for (auto &edge : edges) edge += hit.rowOriginY;
	_lstCrafts->configureCalypsoHdSelectionList(hit.scrollBarWidth,hit.minThumbHeight,hit.rowStride,hit.rowOriginY,hit.dataViewportH,hit.visibleRows,edges);
	list.heading = tr("STR_CAL_BERTHS"); list.emptyTitle = tr("STR_CAL_FLEET_NO_CRAFT");
	list.emptyBody = tr("STR_CAL_FLEET_MAP");
	const Text *headers[] = {_txtCraft,_txtStatus,_txtBase,_txtWeapons};
	for (int i = 0; i < table.columnCount; ++i)
	{
		const auto &c = table.columns[i];
		list.columns.push_back({c.id,headers[i]->getText(),paint(c.rect),{},c.contentRole});
		geo.collectionColumns.push_back(paint(c.rect));
	}
	list.selectedIndex = _picked; list.scrollOffset = _lstCrafts->getScroll(); list.count = _crafts.size();
	list.scroll = {list.scrollOffset,list.count,list.visibleRows,viewport,list.scrollTrack,list.scrollThumb};
	const auto &native = _lstCrafts->getCellTextsSnapshot();
	for (size_t i = 0; i < _crafts.size(); ++i)
	{
		CalypsoHdOperationsRow row;
		row.id = std::to_string(i); row.widget = _lstCrafts; row.state.selected = i == _picked;
		if (i >= list.scrollOffset && i - list.scrollOffset < list.rowSlots.size()) row.rect = list.rowSlots[i - list.scrollOffset];
		for (int j=0; j<table.columnCount; ++j)
		{
			auto value = CommandCenter::calypsoHdNormalizeTtfDisplayText(native[i][j]->getText());
			if (j == 0 && std::find(_selCrafts.begin(),_selCrafts.end(),_crafts[i]) != _selCrafts.end())
				value = "+ " + value;
			row.values.push_back(value);
			row.cells.push_back({value,table.columns[j].contentRole,{true,false,row.state.selected,false}});
		}
		list.rows.push_back(std::move(row));
	}
	auto detailRect = [&](auto r){return paint(r,V::BottomShift,anchor);};
	auto &detail = m.detail;
	detail.id = "ship";
	detail.panel = geo.detailPanel = paint(g.detailPanel,V::BottomShift);
	auto &identity = detail.identity;
	identity.id = "ship"; identity.label = tr("STR_CRAFT");
	identity.title = craft ? craft->getName(language) : std::string(tr("STR_CAL_FLEET_PICK"));
	identity.subtitle = craft ? std::string(tr(craft->getStatus())) : std::string();
	identity.rect = detailRect(g.detail_ship);
	geo.detailIdentity = detailRect(g.detail_ship_label);
	identity.titleRect = geo.detailIdentityTitle = detailRect(g.detail_ship_identity_title);
	identity.subtitleRect = geo.detailIdentitySubtitle = detailRect(g.detail_ship_identity_subtitle);
	geo.detailMetrics = {detailRect(g.detail_ship_metric_fuel),detailRect(g.detail_ship_metric_damage)};
	detail.metrics = {metric("fuel",tr("STR_CAL_FLEET_FUEL"),craft ? std::to_string(craft->getFuelPercentage()) + "%" : "—",geo.detailMetrics[0]),
		metric("damage",tr("STR_DAMAGE"),craft ? std::to_string(craft->getDamagePercentage()) + "%" : "—",geo.detailMetrics[1])};
	auto bindDetail = [&](const char *id, TextButton &button, const std::string &label, auto r, bool enabled) {
		moveOwner(button,frame.resolve(rect(r),V::BottomShift,anchor));
		const auto css = detailRect(r);
		geo.detailActions.push_back(css);
		detail.actions.push_back(detailAction(id,label,css,&button,enabled));
	};
	bindDetail("wing",*_wing,tr(inWing ? "STR_CAL_REMOVE_ESCORT" : "STR_CAL_TOGGLE_WING"),g.detail_ship_action_wing,
		inWing || (canDispatch(craft) && _selCrafts.size()<3));
	bindDetail("info",*_info,tr(craft && craft->getStatus()=="STR_OUT" ? "STR_CAL_FLEET_CENTER" : "STR_INFO"),g.detail_ship_action_info,craft != nullptr);
	bindDetail("reference",*_reference,_reference->getText(),g.detail_ship_action_reference,craft != nullptr);
	auto bindFooter = [&](const char *id, TextButton &button, auto r, bool enabled, const char *tone) {
		moveOwner(button,frame.resolve(rect(r),V::BottomShift));
		const auto css = paint(r,V::BottomShift);
		geo.footerActions.push_back(css);
		auto item = action(id,button.getText(),css,&button,button.getVisible(),tone);
		item.state.disabled = !enabled;
		m.footerActions.push_back(std::move(item));
	};
	bindFooter("base",*_btnGotoBase,g.action_base,_base!=nullptr,"safe");
	bindFooter("cancel",*_btnCancel,g.action_cancel,true,"safe");
	bindFooter("course",*_course,g.action_course,canDispatch(craft),"primary");
	return m;
}
CalypsoHdOperationsModel ConfirmDestinationState::preflightModel()
{
	namespace Course = CalypsoF18CourseConfirmGen;
	using namespace OperationsBindings;
	const auto viewport = calypsoHdOperationsEffectiveViewport();
	const auto mode = classifyCalypsoHdOperationsLayout(viewport.cssWidth, viewport.cssHeight);
	if (mode == CalypsoHdOperationsLayoutClass::Unsupported)
		CalypsoHdUiOverlay::instance().failHdRoute("Course confirmation viewport is unavailable");
	const bool wide = mode == CalypsoHdOperationsLayoutClass::Wide;
	const auto &g = Course::kLayouts[wide ? 0 : 1];
	const auto &table = wide ? Course::kCollectionsWide[0] : Course::kCollectionsCompact[0];
	const auto frame = CalypsoHdOperationsFluidFrame::forReference(g.designWidth, g.designHeight, g.footer.x, g.footer.w);
	auto paint = [&](auto r, V role = V::Fixed, A anchor = A::Geometry) {
		const auto css = frame.cssY(frame.cssX(rect(r), anchor), role);
		return CalypsoHdOperationsRect{css.x, css.y, css.w, css.h};
	};

	auto *lead = _crafts.front();
	auto *language = _game->getLanguage();
	auto *waypoint = dynamic_cast<Waypoint*>(_target);
	const std::string target = waypoint && waypoint->getId() == 0
		? std::string(tr("STR_WAY_POINT")) : _target->getName(language);
	CalypsoHdOperationsModel model;
	model.ownerState = this;
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.familyId = Course::kFamilyId;
	model.suppressedWidgets = {_window, _txtTarget, _txtETA};
	model.visualShell = Course::kVisualShell;
	model.headerArtId = Course::kHeaderArt;
	model.title = tr("STR_CAL_PRE_FLIGHT");
	model.sectionLabel = tr("STR_SELECT_DESTINATION");
	model.baseCaption = tr("STR_BASE");
	model.baseName = lead->getBase()->getName();
	finish(model, _game->getMod(), Course::kPresentationProfile, Course::kProfileId, Course::kProfileVersion,
		Course::kProvenanceTemplate, Course::kProfileStyle, Course::kTypographyWide, Course::kTypographyCompact, wide);
	placeFrame(model, g, frame, paint);
	model.summaryFields = {
		summary("wing", tr("STR_CAL_WING_SELECTED"), std::to_string(_crafts.size()), paint(g.summary_wing), nullptr),
		summary("destination", tr("STR_CAL_FLEET_TARGET"), target, paint(g.summary_destination), _txtTarget)};
	if (!_txtETA->getText().empty())
		model.summaryFields.push_back(summary("eta", tr("STR_CAL_FLEET_ETA"), _txtETA->getText(), paint(g.summary_eta), _txtETA));

	placeTable(model, g, paint);
	auto &list = model.collection;
	auto &geo = model.geometry;
	list.heading = tr("STR_CAL_WING_SELECTED");
	const char *columnKeys[] = {"STR_CRAFT", "STR_CAL_ASSIGNED_PILOTS", "STR_CAL_FLEET_FUEL", "STR_DAMAGE"};
	for (int i = 0; i < table.columnCount; ++i)
	{
		const auto &column = table.columns[i];
		list.columns.push_back({column.id, tr(columnKeys[i]), paint(column.rect), {}, column.contentRole});
		geo.collectionColumns.push_back(paint(column.rect));
	}
	list.count = _crafts.size();
	list.scroll = {0, list.count, list.visibleRows, geo.collectionViewport, list.scrollTrack, list.scrollThumb};
	for (auto *craft : _crafts)
	{
		const auto &personnel = *craft->getBase()->getSoldiers();
		const auto assigned = std::count_if(personnel.begin(), personnel.end(), [craft](Soldier *soldier) {
			return soldier->getCraft() == craft && craft->isPilot(soldier->getId());
		});
		CalypsoHdOperationsRow row;
		row.id = std::to_string(list.rows.size());
		row.widget = _txtTarget;
		if (list.rows.size() < list.rowSlots.size()) row.rect = list.rowSlots[list.rows.size()];
		row.values = {craft->getName(language), std::to_string(assigned) + " / " + std::to_string(craft->getRules()->getPilots()),
			std::to_string(craft->getFuelPercentage()) + "%", std::to_string(craft->getDamagePercentage()) + "%"};
		for (int i = 0; i < table.columnCount; ++i)
			row.cells.push_back({row.values[i], table.columns[i].contentRole, {true, false, false, false}});
		list.rows.push_back(std::move(row));
	}

	const auto anchor = wide ? A::Right : A::Geometry;
	auto inspector = [&](auto r) { return paint(r, V::BottomShift, anchor); };
	auto &detail = model.detail;
	detail.id = "leader";
	detail.panel = geo.detailPanel = paint(g.detailPanel, V::BottomShift);
	auto &identity = detail.identity;
	identity.id = "leader";
	identity.label = tr("STR_CAL_LEAD_SUBMARINE");
	identity.title = lead->getName(language);
	identity.subtitle = tr(lead->getStatus());
	identity.rect = inspector(g.detail_leader);
	geo.detailIdentity = inspector(g.detail_leader_label);
	identity.titleRect = geo.detailIdentityTitle = inspector(g.detail_leader_identity_title);
	identity.subtitleRect = geo.detailIdentitySubtitle = inspector(g.detail_leader_identity_subtitle);
	geo.detailMetrics = {inspector(g.detail_leader_metric_speed), inspector(g.detail_leader_metric_crew)};
	detail.metrics = {
		metric("speed", tr("STR_CAL_FLEET_MAX_SPEED"), std::to_string(lead->getCraftStats().speedMax), geo.detailMetrics[0]),
		metric("crew", tr("STR_PERSONNEL"), std::to_string(lead->getNumTotalSoldiers()), geo.detailMetrics[1])};
	if (_btnFollowWingLeader->getVisible())
	{
		const auto r = inspector(g.detail_leader_action_formation);
		moveOwner(*_btnFollowWingLeader, frame.resolve(rect(g.detail_leader_action_formation), V::BottomShift, anchor));
		auto label = _btnFollowWingLeader->getText() + " " + std::string(tr(_btnFollowWingLeader->getPressed() ? "STR_YES" : "STR_NO"));
		geo.detailActions.push_back(r);
		detail.actions.push_back(detailAction("formation", label, r, _btnFollowWingLeader, true));
	}
	auto footer = [&](const char *id, TextButton *button, auto authored, const char *tone) {
		const auto css = paint(authored, V::BottomShift);
		moveOwner(*button, frame.resolve(rect(authored), V::BottomShift));
		geo.footerActions.push_back(css);
		model.footerActions.push_back(action(id, button->getText(), css, button, button->getVisible(), tone));
	};
	footer("transfer", _btnTransfer, g.action_transfer, "safe");
	footer("cancel", _btnCancel, g.action_cancel, "safe");
	footer("route", _btnOk, g.action_route, "primary");
	return model;
}

}
#endif
