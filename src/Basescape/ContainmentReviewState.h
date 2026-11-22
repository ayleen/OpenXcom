#pragma once
#ifdef __EMSCRIPTEN__
#include "../Engine/State.h"
#include <array>
#include <vector>

namespace OpenXcom
{
class ManageAlienContainmentState;
class Text;
class TextList;
class TextButton;
namespace Calypso { class CalypsoBaseLedger; }

enum class ContainmentAction { Remove, Sell, Cleanup };

class ContainmentReviewState : public State
{
	friend class Calypso::CalypsoBaseLedger;
	ManageAlienContainmentState &_owner;
	ContainmentAction _action;
	Text *_title;
	TextList *_rows;
	TextButton *_confirm, *_cancel;
	Calypso::CalypsoBaseLedger *_ledger = nullptr;
	using Row = std::array<std::string, 3>;
	std::vector<Row> _review;
	std::int64_t _proceeds = 0;
	int _free = 0;
	std::vector<Row> collectReview();
	void showRows();
	void confirm(Action *);
	void cancel(Action *);
public:
	ContainmentReviewState(ManageAlienContainmentState &owner, ContainmentAction action);
	~ContainmentReviewState() override;
	void resize(int &dX, int &dY) override;
};
}
#endif
