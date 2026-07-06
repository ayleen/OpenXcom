#ifdef __EMSCRIPTEN__
#ifndef OPENXCOM_CALYPSOADVISOR_H
#define OPENXCOM_CALYPSOADVISOR_H

/*
 * Phase 39 (Calypso): strategic advisor. Data-driven warnings evaluated once
 * per game day by polling game state. Whole file Emscripten-only. A tripped
 * rule fires the "advisor" tutorial trigger with the rule id, so the popup is
 * an ordinary tutorial step (dedup/persistence/popup reused). Grace streaks
 * are in-memory only (reset on reload).
 */

#include <string>
#include <vector>
#include <map>

namespace OpenXcom
{

class Game;

struct CalypsoAdvisorRule {
	std::string id;
	std::string check;      // idleScientists|idleEngineers|noFacility|
	                        // craftWeaponEquipped|singleBase|facilityBuilt
	std::string checkArg;
	int afterMonth = 0;     // getMonthsPassed() >= afterMonth
	int graceDays = 0;      // consecutive days true before firing
};

class CalypsoAdvisor {
public:
	static CalypsoAdvisor& get();
	void daily(Game* game);            // called once per game day from time1Day
	void resetCounters() { _streak.clear(); }
	void dump() const;                 // logs each rule's gate/eval/streak
private:
	CalypsoAdvisor() {}
	bool evaluate(const Game* game, const CalypsoAdvisorRule& r) const;
	std::map<std::string, int> _streak; // rule id -> consecutive true days
};

} // namespace OpenXcom

#endif // OPENXCOM_CALYPSOADVISOR_H
#endif // __EMSCRIPTEN__
