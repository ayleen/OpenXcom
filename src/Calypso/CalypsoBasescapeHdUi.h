#pragma once
// F01 Basescape HD presentation holder (T12): state-side lifecycle owner for
// exactly one CalypsoHdScreenRenderer. Holder, not painter: geometry, model,
// and paint live in the shared renderer (T14) and the generated contract.
// Whole-file Emscripten guard (Phase 36).
#ifdef __EMSCRIPTEN__

#include <string>
#include <vector>

#include "CalypsoBasescapeHdRuntime.h"

namespace OpenXcom
{
class BasescapeState;
class PlaceFacilityState;
class Base;
class BaseView;
class Game;
class Surface;
class TextButton;
namespace Calypso
{
class CalypsoHdScreenRenderer;

class CalypsoBasescapeHdUi
{
public:
	explicit CalypsoBasescapeHdUi(BasescapeState *state);
	explicit CalypsoBasescapeHdUi(PlaceFacilityState *state);
	~CalypsoBasescapeHdUi();

	static void configure(BasescapeState &state);
	static void configure(PlaceFacilityState &state);
	static bool resize(BasescapeState &state);
	static bool resize(PlaceFacilityState &state);
	static Surface *resolveActionWidget(BasescapeState &state, const std::string &id);
	const std::vector<TextButton *> &railButtons() const { return _railButtons; }

	void refresh();
	void refreshPlacement();
	bool ready() const { return _ready; }
	bool covered() const;
	bool placementMode() const;
	CalypsoHdScreenRenderer *renderer() const { return _renderer; }

	/// Shared read-only Base -> snapshot population for both native host
	/// types (BasescapeState and PlaceFacilityState) and the chooser underlay:
	/// facilities with craft slots, craft visuals, and the base selector.
	/// Chrome strings (name/clock/funds/hover) stay caller-side.
	static void populateBaseVisuals(CalypsoBasescapeHdSnapshot &snapshot,
		Game *game, Base *base, BaseView *view);

private:
	bool checkReadiness() const;
	void ensureRailButtons();
	void feedModel();
	void feedPlacementModel();
	bool applyGeometry();

	BasescapeState *_state;
	PlaceFacilityState *_placementState;
	CalypsoHdScreenRenderer *_renderer;
	bool _ready;
	std::vector<TextButton *> _railButtons;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
