#pragma once
#ifdef __EMSCRIPTEN__
#include "../Engine/InteractiveSurface.h"
#include "CalypsoTabbedManagementRenderer.h"
#include <map>
namespace OpenXcom
{
class State;
class MiniMapView;
class ScannerView;
class Camera;
class SavedBattleGame;
namespace Calypso
{
class CalypsoTacticalSensor final : public CalypsoHdFamilyAdapter
{
  public:
	CalypsoTacticalSensor(State& owner, MiniMapView* map, ScannerView* scanner, Camera* camera,
						  SavedBattleGame& battle, ActionHandler click, const std::string& title);
	~CalypsoTacticalSensor() override;
	const void* topState() const override { return &_owner; }
	void collect(CalypsoHdFrameBuilder& builder) const override;
	void layout();
	void refresh();
	bool closeRequested(Action* action);

  private:
	State& _owner;
	MiniMapView* _map;
	ScannerView* _scanner;
	Camera* _camera;
	SavedBattleGame& _battle;
	std::string _title;
	std::map<std::string, TextButton*> _buttons;
	bool _legend = false;
	std::uint64_t _session = 0;
	CalypsoTabbedModel _model;
	mutable CalypsoSmallConfirmationMotion _motion;
};
}
}
#endif
