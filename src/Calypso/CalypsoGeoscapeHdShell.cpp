#ifdef __EMSCRIPTEN__
#include "CalypsoGeoscapeHdShell.h"

#include "../Geoscape/GeoscapeState.h"
#include "CalypsoGeoscapeHdRuntime.h"
#include "CalypsoViewportRuntime.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Interface/TextButton.h"
#include "../Engine/Logger.h"

namespace OpenXcom
{

namespace
{

struct WidgetRef
{
	Surface **ptr;
};

void applyRect(Surface *widget, const Calypso::CalypsoHdScreenRect& rect)
{
	if (widget == nullptr) return;
	widget->setX(rect.x);
	widget->setY(rect.y);
	widget->setWidth(rect.w);
	widget->setHeight(rect.h);
}

} // namespace

Surface *CalypsoGeoscapeHdShell::resolveWidget(GeoscapeState *s, const std::string& member)
{
	if (member == "btnIntercept") return s->_btnIntercept;
	if (member == "btnBases") return s->_btnBases;
	if (member == "btnGraphs") return s->_btnGraphs;
	if (member == "btnUfopaedia") return s->_btnUfopaedia;
	if (member == "btnOptions") return s->_btnOptions;
	if (member == "btnFunding") return s->_btnFunding;
	if (member == "btn5Secs") return s->_btn5Secs;
	if (member == "btn1Min") return s->_btn1Min;
	if (member == "btn5Mins") return s->_btn5Mins;
	if (member == "btn30Mins") return s->_btn30Mins;
	if (member == "btn1Hour") return s->_btn1Hour;
	if (member == "btn1Day") return s->_btn1Day;
	if (member == "btnZoomIn") return s->_btnZoomIn;
	if (member == "btnZoomOut") return s->_btnZoomOut;
	return nullptr;
}


const char* CalypsoGeoscapeHdShell::apply(GeoscapeState *s)
{
	using namespace OpenXcom::Calypso;
	using namespace CalypsoGeoscapeCommandShellGen;

	const bool listed = s->_game != nullptr && s->_game->getMod() != nullptr
		&& s->_game->getMod()->isHdUiFamilyEnabled("F16");
	const auto decision = calypsoGeoscapeHdGateDecision(true, listed, true, true);
	if (!decision.enabled)
	{
		// Gate off: restore the legacy fillers so the fallback stays exact.
		s->_sidebar->setVisible(true);
		s->_sideLine->setVisible(true);
		if (s->_sideTop) s->_sideTop->setVisible(true);
		if (s->_sideBottom) s->_sideBottom->setVisible(true);
		return decision.reason;
	}

	const auto& metrics = calypsoViewportRuntime().current();
	const bool wide = metrics.layoutClass == CalypsoLayoutClass::Wide;
	const auto* layout = layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (layout == nullptr) return "layout-missing";

	const auto projection = calypsoGeoscapeHdProjection(*layout, metrics);

	int projected = 0;
	int deferred = 0;
	for (const auto& binding : calypsoGeoscapeHdWidgetBindings())
	{
		const std::string role = binding.role;
		if (role.rfind("widget:", 0) != 0) { ++deferred; continue; }
		Surface *widget = resolveWidget(s, role.substr(7));
		if (widget == nullptr) { ++deferred; continue; }
		applyRect(widget, projection.project(binding.actionId));
		++projected;
	}

	// The physical shell replaces the legacy side-panel chrome.
	s->_sidebar->setVisible(false);
	s->_sideLine->setVisible(false);
	if (s->_sideTop) s->_sideTop->setVisible(false);
	if (s->_sideBottom) s->_sideBottom->setVisible(false);

	Log(LOG_INFO) << "[HD] geoscape shell: layout=" << (wide ? "wide" : "compact")
	              << " projected=" << projected << " deferred=" << deferred;
	return decision.reason;
}

} // namespace OpenXcom
#endif // __EMSCRIPTEN__