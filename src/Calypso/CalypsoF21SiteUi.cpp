/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * F21 (Calypso): HD adapter for BuildNewBaseState -- see the header. The
 * strip + generated content block form ONE atomic HD subgroup; the globe
 * renders logically underneath.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoF21SiteUi.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/Screen.h"
#include "../Engine/Surface.h"
#include "../Engine/Unicode.h"
#include "../Geoscape/BuildNewBaseState.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleRegion.h"
#include "../Savegame/Base.h"
#include "../Savegame/Region.h"
#include "../Savegame/SavedGame.h"

#include "CalypsoAbandonPopupUi.h" // hdHarnessDomShow/Hide (shared helpers)
#include "CalypsoF21UiShared.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoTutorialState.h"
#include "CalypsoUiFamilies.h"
#include "CalypsoUiMetrics.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

enum SiteRole : std::uint32_t
{
	ROLE_WINDOW = 1, ROLE_BANNER = 2, ROLE_PROTOCOL = 3, ROLE_TITLE = 4,
	ROLE_SLOT = 5, ROLE_FUNDS = 6, ROLE_COST = 7, ROLE_CARD = 8,
	ROLE_COORDS = 9, ROLE_REGION = 10, ROLE_LEGALITY = 11,
	ROLE_PREVIEW = 12, ROLE_CANCEL = 13, ROLE_DECORATION = 14
};

void applyRect(Surface* surface, const CalypsoF21Rect& rect)
{
	if (!surface) return;
	surface->setX(rect.x);
	surface->setY(rect.y);
	surface->setWidth(rect.width);
	surface->setHeight(rect.height);
}

CalypsoHdPanelStyle f21SiteDetailsStyle()
{
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.shape = CalypsoHdPanelShape::OpposingCutRect;
	s.cutCornerPx = static_cast<float>(CalypsoF21SiteDetailsGen::kCutCornerPx);
	s.borderWidthPx = static_cast<float>(CalypsoF21SiteDetailsGen::kBorderWidthPx);
	s.borderColorRgba = CalypsoF21SiteDetailsGen::kFrameRgba;
	s.fillTopRgba = CalypsoF21SiteDetailsGen::kPanelFillTopRgba;
	s.fillBottomRgba = CalypsoF21SiteDetailsGen::kPanelFillBottomRgba;
	s.gradDirX = 0.18f;
	s.gradDirY = 1.0f;
	return s;
}

int f21SiteDetailsLineCount(bool wide, int cellIndex)
{
	return wide
		? CalypsoF21SiteDetailsGen::kWideLineCounts[cellIndex]
		: CalypsoF21SiteDetailsGen::kCompactLineCounts[cellIndex];
}

    } // namespace

    void CalypsoF21SiteUi::claimLowerGeoscapeShell(CalypsoF21Painter& painter,
        const GeoscapeState* geoscape, std::uint32_t& role)
    {
        if (!geoscape) return;
        painter.claim(geoscape->_sidebar, role++);
        painter.claim(geoscape->_sideLine, role++);
        painter.claim(geoscape->_sideTop, role++);
        painter.claim(geoscape->_sideBottom, role++);
        painter.claim(geoscape->_btnIntercept, role++);
        painter.claim(geoscape->_btnBases, role++);
        painter.claim(geoscape->_btnGraphs, role++);
        painter.claim(geoscape->_btnUfopaedia, role++);
        painter.claim(geoscape->_btnOptions, role++);
        painter.claim(geoscape->_btnFunding, role++);
        painter.claim(geoscape->_btn5Secs, role++);
        painter.claim(geoscape->_btn1Min, role++);
        painter.claim(geoscape->_btn5Mins, role++);
        painter.claim(geoscape->_btn30Mins, role++);
        painter.claim(geoscape->_btn1Hour, role++);
        painter.claim(geoscape->_btn1Day, role++);
        painter.claim(geoscape->_txtFunds, role++);
        painter.claim(geoscape->_txtHour, role++);
        painter.claim(geoscape->_txtHourSep, role++);
        painter.claim(geoscape->_txtMin, role++);
        painter.claim(geoscape->_txtMinSep, role++);
        painter.claim(geoscape->_txtSec, role++);
        painter.claim(geoscape->_txtWeekday, role++);
        painter.claim(geoscape->_txtDay, role++);
        painter.claim(geoscape->_txtMonth, role++);
        painter.claim(geoscape->_txtYear, role++);
    }

    void CalypsoF21SiteUi::collectLowerGeoscapeSuppression(
        const GeoscapeState* geoscape, CalypsoHdLogicalSuppression& suppression)
    {
        if (!geoscape) return;
        suppression.add(geoscape->_sidebar);
        suppression.add(geoscape->_sideLine);
        suppression.add(geoscape->_sideTop);
        suppression.add(geoscape->_sideBottom);
        suppression.add(geoscape->_btnIntercept);
        suppression.add(geoscape->_btnBases);
        suppression.add(geoscape->_btnGraphs);
        suppression.add(geoscape->_btnUfopaedia);
        suppression.add(geoscape->_btnOptions);
        suppression.add(geoscape->_btnFunding);
        suppression.add(geoscape->_btn5Secs);
        suppression.add(geoscape->_btn1Min);
        suppression.add(geoscape->_btn5Mins);
        suppression.add(geoscape->_btn30Mins);
        suppression.add(geoscape->_btn1Hour);
        suppression.add(geoscape->_btn1Day);
        suppression.add(geoscape->_txtFunds);
        suppression.add(geoscape->_txtHour);
        suppression.add(geoscape->_txtHourSep);
        suppression.add(geoscape->_txtMin);
        suppression.add(geoscape->_txtMinSep);
        suppression.add(geoscape->_txtSec);
        suppression.add(geoscape->_txtWeekday);
        suppression.add(geoscape->_txtDay);
        suppression.add(geoscape->_txtMonth);
        suppression.add(geoscape->_txtYear);
    }

CalypsoF21SiteUi::~CalypsoF21SiteUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF21SiteUi::topState() const
{
	return _state;
}

void CalypsoF21SiteUi::collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const
{
	if (!_state) return;
	// The F21 state has its own registered HD chrome; keep every native visual
	// owner suppressed before frame/resource readiness, while retaining its
	// handlers and globe/placement input ownership.
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_btnCancel);
	suppression.add(_state->_hdProtocol);
	suppression.add(_state->_hdSlot);
	suppression.add(_state->_hdFunds);
	suppression.add(_state->_hdCost);
	suppression.add(_state->_hdCard);
	suppression.add(_state->_hdCoords);
	suppression.add(_state->_hdRegion);
	suppression.add(_state->_hdLegality);
	suppression.add(_state->_hdPreview);
	if (!_state || !_state->_game) return;
	collectLowerGeoscapeSuppression(_state->_game->getGeoscapeState(), suppression);
}

void CalypsoF21SiteUi::collect(CalypsoHdFrameBuilder& builder) const
{
	if (!_state || !_state->_hdLayout || !_state->_game) return;

	Mod* mod = _state->_game->getMod();
	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	CalypsoTtfSourceDescriptor mono;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F33_BODY", body)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_MONO", mono)) return;

	const CalypsoHdPresentationMetrics& m = CalypsoHdUiOverlay::instance().frozenMetrics();
	const bool wide = _state->_hdWideLayout;
	const CalypsoF21SiteLayout designLayout = calypsoF21SiteLayout(
		wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

	if (!_presented)
	{
		_presented = true;
		_presentedAtFrame = CalypsoHdUiOverlay::instance().frameId();
	}
	const CalypsoF21Motion motion(_presented, _presentedAtFrame,
		CalypsoF21SiteGen::kMotionDurationMs, CalypsoF21SiteGen::kMotionScaleFrom);

	// One atomic subgroup: strip + card show physically together or not at
	// all. The globe stays logical; NO backdrop dim (the globe is the task).
	builder.beginSubgroup();
	const CalypsoLogicalRect bannerLogical = f21WidgetRect(_state->_window);
	const double uiScale = designLayout.banner.width > 0
		? (double)bannerLogical.w / designLayout.banner.width : 1.0;
	const CalypsoLogicalRect winFull{
		bannerLogical.x + (int)std::llround(
			(designLayout.window.x - designLayout.banner.x) * uiScale),
		bannerLogical.y + (int)std::llround(
			(designLayout.window.y - designLayout.banner.y) * uiScale),
		std::max(1, (int)std::llround(designLayout.window.width * uiScale)),
		std::max(1, (int)std::llround(designLayout.window.height * uiScale)) };
	CalypsoF21Painter p{ builder, kF21FamilyId,
		reinterpret_cast<std::uintptr_t>(_state), 0, motion.opacity, motion.scale,
		CalypsoF21Rect{ winFull.x, winFull.y, winFull.w, winFull.h },
		m.scaleX, m.scaleY };
	p.winLogical = winFull;
	p.windowDesign = designLayout.window;
	p.uiScale = uiScale;
	const double titlePx = (wide ? CalypsoHdThemeGen::kF21TitleWidePx : CalypsoHdThemeGen::kF21TitleCompactPx)
		* CalypsoF21SiteGen::kEngineTextScaleTitle;
	const double dataPx = (wide ? CalypsoHdThemeGen::kF21DataWidePx : CalypsoHdThemeGen::kF21DataCompactPx)
		* CalypsoF21SiteGen::kEngineTextScaleData;
	const double bodyPx = (wide ? CalypsoHdThemeGen::kF21BodyWidePx : CalypsoHdThemeGen::kF21BodyCompactPx)
		* CalypsoF21SiteGen::kEngineTextScaleBody;
	const double actionPx = (wide ? CalypsoHdThemeGen::kF21ActionWidePx : CalypsoHdThemeGen::kF21ActionCompactPx)
		* CalypsoF21SiteGen::kEngineTextScaleAction;

	// Floating command strip + soft shadow only (no halo:
	// this is chrome, not a modal). Same opposing-cut language as the modals.
	{
		const CalypsoLogicalRect b = f21WidgetRect(_state->_window);
		p.styled(CalypsoLogicalRect{ b.x, b.y + 4, b.w, b.h },
			f21GlowStyle(CalypsoHdTheme::kShadowGlow, CalypsoHdTheme::kShadowGlowRadiusPx),
			nullptr, ROLE_BANNER);
		p.styled(b, f21WindowStyle(), _state->_window, ROLE_BANNER);
		const CalypsoLogicalRect status = f21WidgetRect(_state->_hdProtocol);
		p.decoration(CalypsoLogicalRect{ status.x, status.y + status.h - 1, status.w, 1 },
			kF21DividerRgba, ROLE_DECORATION);
	}

	// Generated titleless 2x2 content block (bottom-left). Its frame, fill,
	// content-sized columns and internal dividers come from one generated contract.
	{
		const CalypsoLogicalRect c = f21WidgetRect(_state->_hdCard);
		p.styled(c, f21SiteDetailsStyle(), _state->_hdCard, ROLE_CARD);
		p.decoration(p.project(designLayout.rowDivider),
			CalypsoF21SiteDetailsGen::kDividerRgba, ROLE_DECORATION);
		p.decoration(p.project(designLayout.columnDivider),
			CalypsoF21SiteDetailsGen::kDividerRgba, ROLE_DECORATION);
	}

	// Cancel (additional bases only; the widget is hidden for the first base
	// and hidden widgets submit nothing).
	if (_state->_btnCancel && _state->_btnCancel->getVisible())
	{
		p.styled(f21WidgetRect(_state->_btnCancel), f21ButtonStyleFor(
			CalypsoActionTone::Safe, f21ButtonVisualState(_state->_btnCancel)),
			_state->_btnCancel, ROLE_CANCEL);
		p.text(_state->_btnCancel, heading, _state->_btnCancel->getText(), CalypsoHdTheme::kNearWhite,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_CANCEL,
			CalypsoHdTheme::kLabelTrackingEm,
			actionPx);
	}

    // The lower Geoscape remains the input owner, but its replaced shell is
    // claimed in this same atomic subgroup. The globe and placement widgets
    // are intentionally not in this set.
    std::uint32_t lowerRole = ROLE_DECORATION + 1;
    claimLowerGeoscapeShell(p, _state->_game->getGeoscapeState(), lowerRole);

    p.textRect(f21ProtocolTextRect(_state->_hdProtocol, wide), _state->_hdProtocol,
		mono, _state->_hdProtocol->getText(), kF21ProtocolTextRgba,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_PROTOCOL, 0.10,
		wide ? CalypsoHdThemeGen::kF21ProtocolWidePx : CalypsoHdThemeGen::kF21ProtocolCompactPx);
	p.text(_state->_txtTitle, heading, _state->_txtTitle->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_TITLE,
		CalypsoHdTheme::kTitleTrackingEm,
		titlePx);
	p.text(_state->_hdSlot, mono, _state->_hdSlot->getText(), CalypsoHdThemeGen::kAccent,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_SLOT, 0.0,
		dataPx);
	p.text(_state->_hdFunds, mono, _state->_hdFunds->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_FUNDS, 0.0,
		dataPx);
	p.text(_state->_hdCost, mono, _state->_hdCost->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_COST, 0.0,
		dataPx);
	p.text(_state->_hdCoords, mono, _state->_hdCoords->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Top, f21SiteDetailsLineCount(wide, 0), ROLE_COORDS, 0.0,
		dataPx, false);
	p.text(_state->_hdRegion, mono, _state->_hdRegion->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Top, f21SiteDetailsLineCount(wide, 1), ROLE_REGION, 0.0,
		dataPx, false);
	p.text(_state->_hdLegality, body, _state->_hdLegality->getText(), CalypsoHdThemeGen::kGold,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Top, f21SiteDetailsLineCount(wide, 2), ROLE_LEGALITY, 0.0,
		bodyPx, false);
	p.text(_state->_hdPreview, body, _state->_hdPreview->getText(), kF21MutedBodyRgba,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Top, f21SiteDetailsLineCount(wide, 3), ROLE_PREVIEW, 0.0,
		bodyPx, false);
}

void CalypsoF21SiteUi::applyRects(BuildNewBaseState& state, const CalypsoF21SiteLayout& layout)
{
	applyRect(state._window, layout.banner);
	applyRect(state._hdProtocol, layout.status);
	applyRect(state._txtTitle, layout.title);
	applyRect(state._hdSlot, layout.slot);
	applyRect(state._hdFunds, layout.funds);
	applyRect(state._hdCost, layout.cost);
	applyRect(state._hdCard, layout.card);
	applyRect(state._hdCoords, layout.coords);
	applyRect(state._hdRegion, layout.region);
	applyRect(state._hdLegality, layout.legality);
	applyRect(state._hdPreview, layout.preview);
	applyRect(state._btnCancel, layout.cancel);
}

void CalypsoF21SiteUi::configure(BuildNewBaseState& state, bool allowPhysicalOverlay)
{
	state._hdLayout = state._game && state._game->getMod()
		&& state._game->getLanguage()
		&& isF34PhysicalRouteEligible(
			state._game->getMod()->isHdUiFamilyEnabled("F21"),
			state._game->getLanguage()->getTextDirection() == DIRECTION_RTL,
			!allowPhysicalOverlay);
	if (!state._hdLayout) return;

	state._hdWideLayout = currentF21LayoutClass() == CalypsoLayoutClass::Wide;
	const CalypsoF21SiteLayout layout = calypsoF21SiteLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

	state._txtTitle->setText(state.tr("STR_CAL_F21_SELECT_SITE"));

	// HD-only readouts. The card/coords/region readouts update from the live
	// hover snapshot in think().
	state._hdProtocol = new Text(1, 1, 0, 0);
	state.add(state._hdProtocol);
	state._hdProtocol->setText(state.tr("STR_CAL_F21_PROTOCOL_SITE"));

	state._hdSlot = new Text(1, 1, 0, 0);
	state.add(state._hdSlot);
	state._hdSlot->setText(state.tr("STR_CAL_F21_BASE_SLOT")
		.arg((int)state._game->getSavedGame()->getBases()->size() + 1).arg(8));

	state._hdFunds = new Text(1, 1, 0, 0);
	state.add(state._hdFunds);
	state._hdFunds->setText(state.tr("STR_CAL_F21_AVAILABLE_FUNDS")
		.arg(Unicode::formatFunding(state._game->getSavedGame()->getFunds())));

	state._hdCost = new Text(1, 1, 0, 0);
	state.add(state._hdCost);
	state._hdCost->setText(state.tr("STR_CAL_F21_REGION_COST_TBD"));

	state._hdCard = new Text(1, 1, 0, 0);
	state.add(state._hdCard);

	state._hdCoords = new Text(1, 1, 0, 0);
	state.add(state._hdCoords);
	state._hdCoords->setText(state.tr("STR_CAL_F21_CANDIDATE_SITE"));

	state._hdRegion = new Text(1, 1, 0, 0);
	state.add(state._hdRegion);
	state._hdRegion->setText(state.tr("STR_CAL_F21_REGION_PENDING"));

	state._hdLegality = new Text(1, 1, 0, 0);
	state.add(state._hdLegality);
	state._hdLegality->setText(state.tr("STR_CAL_F21_SITE_LEGAL"));

	state._hdPreview = new Text(1, 1, 0, 0);
	state.add(state._hdPreview);
	state._hdPreview->setText(state.tr("STR_CAL_F21_SITE_PREVIEW"));

	// The clean harness compares deterministic fixture content. Populate the
	// same candidate represented by the canonical DOM reference instead of
	// leaving one renderer at PENDING while the other shows resolved data.
	if (calypsoHarnessHostUp(calypsoHarnessSession()) && state._base)
	{
		const double lat = state._base->getLatitude() * 180.0 / M_PI;
		const double lon = state._base->getLongitude() * 180.0 / M_PI;
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(1)
		   << std::fabs(lat) << "·" << (lat >= 0 ? "N" : "S")
		   << "  " << std::fabs(lon) << "·" << (lon >= 0 ? "E" : "W");
		state._hdCoords->setText(state.tr("STR_CAL_F21_COORDINATES").arg(ss.str()));
		for (const auto* region : *state._game->getSavedGame()->getRegions())
		{
			if (region->getRules()->insideRegion(state._base->getLongitude(), state._base->getLatitude()))
			{
				state._hdRegion->setText(state.tr("STR_CAL_F21_REGION").arg(
					state.tr(region->getRules()->getType())));
				state._hdCost->setText(state.tr("STR_CAL_F21_BUILD_COST").arg(
					Unicode::formatFunding(region->getRules()->getBaseCost())));
				break;
			}
		}
	}

	CalypsoF21SiteUi::applyRects(state, layout);
	state.enableUiScaling(layout.designWidth, layout.designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);

	// Site clicks inside the taller HD strip must not place a base.
	state._hdStripBottom = layout.banner.y + layout.banner.height;

	CalypsoF21SiteUi* adapter = new CalypsoF21SiteUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);

	if (calypsoHarnessHostUp(calypsoHarnessSession()))
	{
		hdHarnessDomShow();
	}
}

bool CalypsoF21SiteUi::resize(BuildNewBaseState& state)
{
	if (!state._hdLayout) return false;

	const bool wide = currentF21LayoutClass() == CalypsoLayoutClass::Wide;
	if (wide != state._hdWideLayout)
	{
		state._hdWideLayout = wide;
		const CalypsoF21SiteLayout layout = calypsoF21SiteLayout(
			wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
		CalypsoF21SiteUi::applyRects(state, layout);
		state.recaptureUiScaling(layout.designWidth, layout.designHeight, 1.0f,
			/*subtractVanillaCenter=*/false);
		state._hdStripBottom = layout.banner.y + layout.banner.height;
	}
	else
	{
		state.applyUiScaling();
	}
	return true;
}

void CalypsoF21SiteUi::refreshHoverReadouts(BuildNewBaseState& state, double lon, double lat)
{
	// Region + region-defined base cost follow the pointer (F21 hover card).
	if (!state._hdLayout || lon != lon || lat != lat
		|| !state._hdCoords || !state._hdRegion || !state._hdCost)
	{
		return;
	}
	std::ostringstream ss;
	ss << std::fixed << std::setprecision(1)
		<< std::fabs(lat * 180.0 / M_PI) << "·" << (lat >= 0 ? "N" : "S")
		<< "  " << std::fabs(lon * 180.0 / M_PI) << "·" << (lon >= 0 ? "E" : "W");
	state._hdCoords->setText(state.tr("STR_CAL_F21_COORDINATES").arg(ss.str()));
	for (const auto* region : *state._game->getSavedGame()->getRegions())
	{
		if (region->getRules()->insideRegion(lon, lat))
		{
			state._hdRegion->setText(state.tr("STR_CAL_F21_REGION").arg(
				state.tr(region->getRules()->getType())));
			state._hdCost->setText(state.tr("STR_CAL_F21_BUILD_COST").arg(
				Unicode::formatFunding(region->getRules()->getBaseCost())));
			break;
		}
	}
}

} // namespace Calypso

void BuildNewBaseState::blit()
{
	const bool tutorial = dynamic_cast<CalypsoTutorialState *>(_game->getTopState()) != nullptr;
	const auto& overlay = Calypso::CalypsoHdUiOverlay::instance();
	const std::uint64_t frameId = overlay.frameId();
	const bool coveredByHdPopup = overlay.logicalWidgetSuppressed(_window, frameId)
		|| overlay.logicalWidgetSuppressed(_txtTitle, frameId)
		|| overlay.logicalWidgetSuppressed(_btnCancel, frameId);
	if (!tutorial && !coveredByHdPopup)
	{
		State::blit();
		return;
	}

	// A DOM tutorial and a modal HD popup both suppress only this state's site
	// chrome; the globe and navigation controls remain the live context behind
	// them. Popup suppression is registered before physical readiness, so the
	// vanilla Site window cannot flash through during the opening animation.
	SDL_Surface *screen = _game->getScreen()->getSurface();
	for (auto *surface : _surfaces)
	{
		const bool siteChrome = surface == _window || surface == _txtTitle || surface == _btnCancel
			|| surface == _hdProtocol || surface == _hdSlot || surface == _hdFunds
			|| surface == _hdCost || surface == _hdCard || surface == _hdCoords
			|| surface == _hdRegion || surface == _hdLegality || surface == _hdPreview;
		if (siteChrome && (tutorial || overlay.logicalWidgetSuppressed(surface, frameId)))
			continue;
		surface->blit(screen);
	}
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
