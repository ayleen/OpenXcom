#pragma once
#ifdef __EMSCRIPTEN__
#include "../Engine/State.h"
namespace OpenXcom {
namespace Calypso { class CalypsoBaseLedger; }
class TextList;
class TextButton;

class CalypsoTrainingBases : public State
{
public:
	explicit CalypsoTrainingBases(bool psi);
	~CalypsoTrainingBases() override;
	void init() override;
	void resize(int &dx, int &dy) override;

private:
	friend class Calypso::CalypsoBaseLedger;
	void populate();
	void changeMode(Action *action);
	void openBase(Action *action);
	void close(Action *action);
	TextList *_rows;
	TextButton *_physical, *_psionic, *_done;
	Calypso::CalypsoBaseLedger *_ledger = nullptr;
	bool _psi;
	const bool _allowPsiAssignment;
	int _capacity = 0, _used = 0;
};
}
#endif
