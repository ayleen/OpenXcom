#include "../Basescape/TechTreeSelectState.h"
#include "../Basescape/TechTreeViewerState.h"
#include "../Engine/Game.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "CalypsoBaseLedger.h"

namespace OpenXcom
{
namespace
{
const char *const categoryKeys[] = {"STR_ALL", "STR_CAL_TYPE_SCIENCE", "STR_CAL_TYPE_ENGINEERING",
	"STR_CAL_TYPE_FACILITIES", "STR_CAL_TYPE_ITEMS", "STR_CAL_TYPE_CRAFT"};
enum TopicStatus { All, Discovered, Undiscovered, Disabled, Available, Locked };
const char *const statusKeys[] = {"STR_ALL", "STR_CAL_TOPIC_DISCOVERED", "STR_CAL_TOPIC_UNDISCOVERED",
	"STR_CAL_TOPIC_DISABLED", "STR_CAL_TOPIC_AVAILABLE", "STR_CAL_TOPIC_LOCKED"};

TopicStatus statusOf(const TechTreeViewerState &viewer, int type, const std::string &id)
{
	switch (type)
	{
	case TTV_RESEARCH:
		if (viewer.isResearchDisabled(id)) return Disabled;
		return viewer.isDiscoveredResearch(id) ? Discovered : Undiscovered;
	case TTV_MANUFACTURING: return viewer.isDiscoveredManufacture(id) ? Available : Locked;
	case TTV_FACILITIES: return viewer.isDiscoveredFacility(id) ? Available : Locked;
	case TTV_ITEMS: return viewer.isProtectedAndDiscoveredItem(id) ? Available : Locked;
	case TTV_CRAFTS: return viewer.isDiscoveredCraft(id) ? Available : Locked;
	default: return Locked;
	}
}
}

void TechTreeSelectState::configureSelector()
{
	_screen = true;
	_category = new TextButton(64, 16, 0, 0);
	_status = new TextButton(64, 16, 0, 0);
	_open = new TextButton(64, 16, 0, 0);
	for (auto *button : {_category, _status, _open}) add(button, "button", "techTreeSelect");
	_category->onMouseClick((ActionHandler)&TechTreeSelectState::chooseCategory);
	_status->onMouseClick((ActionHandler)&TechTreeSelectState::chooseStatus);
	_open->setText(tr("STR_CAL_OPEN_TOPIC"));
	_open->onMouseClick((ActionHandler)&TechTreeSelectState::openChosen);
	_btnOk->setText(tr("STR_CANCEL"));
	_btnQuickSearch->onChange((ActionHandler)&TechTreeSelectState::btnQuickSearchApply);
	_lstTopics->setColumns(3, 110, 50, 38);
	updateResults();
	_hdLedger = new Calypso::CalypsoBaseLedger(*this);
}

void TechTreeSelectState::updateResults()
{
	_displayed.clear();
	_lstTopics->clearList();
	_chosen = static_cast<size_t>(-1);
	_category->setText(std::string(tr("STR_CAL_TOPIC_TYPE")) + " · " + std::string(tr(categoryKeys[_categoryFilter])));
	_status->setText(std::string(tr("STR_CAL_TOPIC_STATUS")) + " · " + std::string(tr(statusKeys[_statusFilter])));
	for (size_t i = 0; i < _availableTopics.size(); ++i)
	{
		const int type = topicType(i);
		if (_categoryFilter != 0 && type != _categoryFilter) continue;
		const auto &id = _availableTopics[i];
		const auto status = statusOf(*_parent, type, id);
		if (_statusFilter != All && status != _statusFilter) continue;
		_lstTopics->addRow(3, tr(id).c_str(), tr(categoryKeys[type]).c_str(), tr(statusKeys[status]).c_str());
		_displayed.push_back(i);
	}
}

void TechTreeSelectState::chooseCategory(Action *)
{
	_categoryFilter = (_categoryFilter + 1) % (TTV_CRAFTS + 1);
	updateResults();
}

void TechTreeSelectState::chooseStatus(Action *)
{
	_statusFilter = (_statusFilter + 1) % (Locked + 1);
	updateResults();
}

void TechTreeSelectState::openChosen(Action *)
{
	if (_chosen >= _availableTopics.size()) return;
	_parent->setSelectedTopic(_availableTopics[_chosen], static_cast<TTVMode>(topicType(_chosen)));
	_game->popState();
}

void TechTreeSelectState::resize(int &dX, int &dY)
{
	if (_hdLedger) _hdLedger->refresh();
	else State::resize(dX, dY);
}
}
