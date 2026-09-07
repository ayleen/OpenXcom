#pragma once
// F01 Basescape HD presentation holder (T12): state-side lifecycle owner for
// exactly one CalypsoHdScreenRenderer. Holder, not painter: geometry, model,
// and paint live in the shared renderer (T14) and the generated contract.
// Whole-file Emscripten guard (Phase 36).
#ifdef __EMSCRIPTEN__

#include <string>
#include <vector>

namespace OpenXcom
{
class BasescapeState;
class Surface;
class TextButton;
namespace Calypso
{
class CalypsoHdScreenRenderer;

class CalypsoBasescapeHdUi
{
public:
	explicit CalypsoBasescapeHdUi(BasescapeState *state);
	~CalypsoBasescapeHdUi();

	static void configure(BasescapeState &state);
	static bool resize(BasescapeState &state);
	static Surface *resolveActionWidget(BasescapeState &state, const std::string &id);
	const std::vector<TextButton *> &railButtons() const { return _railButtons; }

	void refresh();
	bool ready() const { return _ready; }
	bool covered() const;
	CalypsoHdScreenRenderer *renderer() const { return _renderer; }

private:
	bool checkReadiness() const;
	void ensureRailButtons();
	void feedModel();
	bool applyGeometry();

	BasescapeState *_state;
	CalypsoHdScreenRenderer *_renderer;
	bool _ready;
	std::vector<TextButton *> _railButtons;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
