#ifdef __EMSCRIPTEN__
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "../Engine/State.h"
#include <string>
#include <vector>
#include <cstdint>
namespace OpenXcom {
#ifdef __EMSCRIPTEN__
namespace Calypso { class CalypsoF36MarketUi; }
#endif
class TextButton; class Window; class Text; class TextList; class Base;
class CalypsoMarketState : public State
{
private:
	friend class Calypso::CalypsoF36MarketUi;
	Base* _base;
	bool _sellMode;
	Window* _window;
	Text* _txtTitle, *_txtInfo;
	TextButton* _btnCancel;
	TextList* _lstCounterparties;
	std::vector<std::string> _rowCp;   // row -> counterparty id (row-aligned with the list)
#ifdef __EMSCRIPTEN__
	bool _hdLayout = false;
	bool _hdWideLayout = false;
	Calypso::CalypsoF36MarketUi *_hdAdapter = nullptr;
	std::uint64_t _hdHarnessGeneration = 0;
#endif
	void refresh();
public:
	CalypsoMarketState(Base* base, bool sellMode);
	~CalypsoMarketState() override;
	void init() override;
#ifdef __EMSCRIPTEN__
	void resize(int &dX, int &dY) override;
#endif
	void btnCancelClick(Action* action);
	void lstCounterpartyClick(Action* action);
};
}
#endif
