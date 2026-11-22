#ifdef __EMSCRIPTEN__
#include "CalypsoF18PatrolArrivalUi.h"
#include "CalypsoFleetAlertLayout.h"
#include "Generated/CalypsoF18PatrolArrival.generated.h"
#include "../Engine/Game.h"
#include "../Geoscape/CraftPatrolState.h"
#include "../Geoscape/GeoscapeState.h"

namespace OpenXcom { namespace Calypso {
namespace G = CalypsoF18PatrolArrivalGen;
CalypsoF18PatrolArrivalUi::~CalypsoF18PatrolArrivalUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}
const void *CalypsoF18PatrolArrivalUi::topState() const { return _state; }

void CalypsoF18PatrolArrivalUi::collect(CalypsoHdFrameBuilder &builder) const
{
	auto &s = *_state;
	const int index = s._hdWideLayout ? 0 : 1;
	CalypsoSmallConfirmationModel m;
	m.instance = _state;
	m.mod = s._game->getMod();
	m.wide = s._hdWideLayout;
	m.familyId = G::kFamilyId;
	m.protocolText = G::kProtocol;
	m.titleText = s.tr("STR_NOW_PATROLLING");
	m.visualScale = G::kPresentationScale;
	m.cutCornerPx = G::kCutCornerPx;
	m.protocolTextInsetPx = G::kProtocolTextInsetPx;
	m.panelFillTop = G::kPanelFillTop;
	m.panelFillBottom = G::kPanelFillBottom;
	m.frameColor = G::kFrame;
	m.protocolColor = G::kProtocolText;
	m.dividerColor = G::kDivider;
	m.footerDotColor = G::kFooterDot;
	m.warningColor = G::kWarning;
	m.motionDurationMs = G::kMotionDurationMs;
	m.motionScaleFrom = G::kMotionScaleFrom;
	calypsoCollectFleetAlert(builder, m, _motion, G::kLayouts[index], G::kButtonRects[index],
		G::kButtons, *s._window, *s._txtDestination, s._txtPatrolling, *s._btnOk, *s._btnRedirect);
}

void CalypsoF18PatrolArrivalUi::configure(CraftPatrolState &s, bool allow)
{
	if (!allow || !s._game || !s._game->getMod())
		CalypsoHdUiOverlay::instance().failHdRoute("Fleet alert requires a loaded mod");
	auto *ui = new CalypsoF18PatrolArrivalUi(&s);
	auto &overlay = CalypsoHdUiOverlay::instance();
	const auto *globe = dynamic_cast<GeoscapeState*>(s._game->getTopState());
	ui->_underlay = overlay.hasRegisteredState(globe) ? globe : nullptr;
	s._screen = !ui->_underlay;
	s._hdAdapter = ui;
	s._hdLayout = true;
	resize(s);
	overlay.registerAdapter(ui);
	if (calypsoHarnessHostUp(calypsoHarnessSession())) calypsoHdHarnessDomShow();
}

bool CalypsoF18PatrolArrivalUi::resize(CraftPatrolState &s)
{
	if (!s._hdLayout) return false;
	s._hdWideLayout = calypsoFleetAlertWide();
	const int index = s._hdWideLayout ? 0 : 1;
	const auto &g = G::kLayouts[index];
	calypsoPlaceFleetAlert(g, G::kButtonRects[index], *s._window, *s._txtDestination,
		s._txtPatrolling, *s._btnOk, *s._btnRedirect);
	s.recaptureUiScaling(g.designWidth, g.designHeight, 1.0f, false);
	return true;
}
} }
#endif
