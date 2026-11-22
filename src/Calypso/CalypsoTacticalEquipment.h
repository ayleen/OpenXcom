#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoTabbedManagementRenderer.h"
#include <map>
namespace OpenXcom
{
class AlienInventoryState;
class InventoryState;
class State;
class Action;
namespace Calypso
{
class CalypsoTacticalEquipment final : public CalypsoHdFamilyAdapter
{
  public:
	explicit CalypsoTacticalEquipment(InventoryState& inventory);
	explicit CalypsoTacticalEquipment(AlienInventoryState& inspection);
	~CalypsoTacticalEquipment() override;
	const void* topState() const override { return &_owner; }
	void layout();
	void click(Action* action);
	void collect(CalypsoHdFrameBuilder& builder) const override;
	void refresh();

  private:
	State& _owner;
	InventoryState* _inventory = nullptr;
	AlienInventoryState* _inspection = nullptr;
	CalypsoTabbedModel _model;
	std::map<std::string, TextButton*> _actions;
	bool _details = false;
	std::uint64_t _revision = 0;
	const void* _held = nullptr;
	int _heldX = 0, _heldY = 0;
	std::uint64_t _session = 0;
	mutable CalypsoSmallConfirmationMotion _motion;
	void open();
	void updateData(CalypsoTabbedModel& model) const;
};
}
}
#endif
