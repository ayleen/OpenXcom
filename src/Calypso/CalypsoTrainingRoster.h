#pragma once
#ifdef __EMSCRIPTEN__
#include "../Engine/State.h"
#include "../Engine/InteractiveSurface.h"
#include <functional>
#include <string>
#include <vector>

namespace OpenXcom {
namespace Calypso { class CalypsoBaseLedger; }
class Base;
class Soldier;
class TextList;
class TextButton;

class CalypsoTrainingRoster : public State
{
protected:
	void configureRoster(Base *base, const char *style, std::vector<std::string> labels,
		ActionHandler assign, ActionHandler clear);
	void rosterOrder(Action *);
	void rosterDetails(Action *);
	Base *_rosterBase = nullptr;
	Soldier *_selectedDiver = nullptr;
	bool _reverseOrder = false;
	std::vector<std::string> _sortLabels;
	std::function<size_t()> _sortIndex;
	std::function<void(size_t, bool)> _applySort;
	std::function<void(size_t, int)> _moveDiver;
	TextButton *_order, *_details, *_assign, *_clear;
	Calypso::CalypsoBaseLedger *_ledger = nullptr;
	~CalypsoTrainingRoster() override;
	void resize(int &dx, int &dy) override;
private:
	friend class CalypsoTrainingOrder;
	friend class Calypso::CalypsoBaseLedger;
	size_t diverIndex() const;
};

class CalypsoTrainingOrder : public State
{
public:
	explicit CalypsoTrainingOrder(CalypsoTrainingRoster &roster);
	~CalypsoTrainingOrder() override;
	void resize(int &dx, int &dy) override;
private:
	friend class Calypso::CalypsoBaseLedger;
	void populate();
	void sort(Action *);
	void reverse(Action *);
	void move(Action *);
	void close(Action *);
	CalypsoTrainingRoster &_roster;
	TextList *_rows;
	TextButton *_reverse, *_up, *_down, *_done;
	Calypso::CalypsoBaseLedger *_ledger = nullptr;
};
}
#endif
