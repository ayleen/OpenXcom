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
#include "GraphsState.h"
#include <sstream>
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoStrategicGraphs.h"
#endif
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/Palette.h"
#include "../Engine/Surface.h"
#include "../Engine/InteractiveSurface.h"
#include "../Savegame/Country.h"
#include "../Savegame/Region.h"
#include "../Mod/RuleCountry.h"
#include "../Mod/RuleRegion.h"
#include "../Interface/Text.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/ToggleTextButton.h"
#include "../Savegame/GameTime.h"
#include "../Savegame/SavedGame.h"
#include "../Interface/TextList.h"
#include "../Engine/Action.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Mod/RuleInterface.h"

namespace OpenXcom
{

struct GraphButInfo
{
	std::string _id;
	LocalizedText _name;
	int _color;
	bool _pushed;
	GraphButInfo(const std::string &id, const LocalizedText& name, Uint8 color): _id(id), _name(name), _color(color), _pushed(false) {}
};
/**
 * Initializes all the elements in the Graphs screen.
 * @param game Pointer to the core game.
 */
GraphsState::GraphsState() : _butRegionsOffset(0), _butCountriesOffset(0), _zoom(100)
{
	// Create object
	_bg = new InteractiveSurface(320, 200, 0, 0);
	_bg->onMousePress((ActionHandler)&GraphsState::shiftButtons, SDL_BUTTON_WHEELUP);
	_bg->onMousePress((ActionHandler)&GraphsState::shiftButtons, SDL_BUTTON_WHEELDOWN);
	_btnUfoRegion = new InteractiveSurface(32, 24, 96, 0);
	_btnUfoCountry = new InteractiveSurface(32, 24, 128, 0);
	_btnXcomRegion = new InteractiveSurface(32, 24, 160, 0);
	_btnXcomCountry = new InteractiveSurface(32, 24, 192, 0);
	_btnIncome = new InteractiveSurface(32, 24, 224, 0);
	_btnFinance = new InteractiveSurface(32, 24, 256, 0);
	_btnGeoscape = new InteractiveSurface(32, 24, 288, 0);
	_txtTitle = new Text(230, 16, 90, 28);
	_txtFactor = new Text(38, 11, 96, 28);
	_txtMonths = new TextList(205, 8, 115, 183);
	_txtYears = new TextList(200, 8, 121, 191);

	// Set palette
	setInterface("graphs");

	//add all our element
	add(_bg);
	add(_btnUfoRegion);
	add(_btnUfoCountry);
	add(_btnXcomRegion);
	add(_btnXcomCountry);
	add(_btnIncome);
	add(_btnFinance);
	add(_btnGeoscape);
	add(_txtMonths, "scale", "graphs");
	add(_txtYears, "scale", "graphs");
	add(_txtTitle, "text", "graphs");
	add(_txtFactor, "text", "graphs");
	for (int scaleText = 0; scaleText != 10; ++scaleText)
	{
		_txtScale.push_back(new Text(42, 16, 80, 171 - (scaleText*14)));
		add(_txtScale.at(scaleText), "scale", "graphs");
	}
	Uint8 regionTotalColor = _game->getMod()->getInterface("graphs")->getElement("regionTotal")->color;
	Uint8 countryTotalColor = _game->getMod()->getInterface("graphs")->getElement("countryTotal")->color;

	//create buttons (sooooo many buttons)
	size_t offset = 0;
	for (auto* region : *_game->getSavedGame()->getRegions())
	{
		// always save in toggles all the region
		Uint8 color = 13 + 8 * (offset % GRAPH_MAX_BUTTONS);
		_regionToggles.push_back(new GraphButInfo("rule/" + region->getRules()->getType(), tr(region->getRules()->getType()), color));
		// initially add the GRAPH_MAX_BUTTONS having the first regions information
		if (offset < GRAPH_MAX_BUTTONS)
		{
			_btnRegions.push_back(new ToggleTextButton(88, 11, 0, offset*11));
			_btnRegions.at(offset)->setText(tr(region->getRules()->getType()));
			_btnRegions.at(offset)->setInvertColor(color);
			_btnRegions.at(offset)->onMousePress((ActionHandler)&GraphsState::btnRegionListClick);
			add(_btnRegions.at(offset), "button", "graphs");
		}
		_alienRegionLines.push_back(new Surface(320,200,0,0));
		add(_alienRegionLines.at(offset));
		_xcomRegionLines.push_back(new Surface(320,200,0,0));
		add(_xcomRegionLines.at(offset));

		++offset;
	}

	if (_regionToggles.size() < GRAPH_MAX_BUTTONS)
		_btnRegionTotal = new ToggleTextButton(88, 11, 0, _regionToggles.size()*11);
	else
		_btnRegionTotal = new ToggleTextButton(88, 11, 0, GRAPH_MAX_BUTTONS*11);
	_regionToggles.push_back(new GraphButInfo("total", tr("STR_TOTAL_UC"), regionTotalColor));
	_btnRegionTotal->onMousePress((ActionHandler)&GraphsState::btnRegionListClick);
	_btnRegionTotal->setInvertColor(regionTotalColor);
	_btnRegionTotal->setText(tr("STR_TOTAL_UC"));
	_alienRegionLines.push_back(new Surface(320,200,0,0));
	add(_alienRegionLines.at(offset));
	_xcomRegionLines.push_back(new Surface(320,200,0,0));
	add(_xcomRegionLines.at(offset));
	add(_btnRegionTotal, "button", "graphs");

	offset = 0;
	for (auto* country : *_game->getSavedGame()->getCountries())
	{
		// always save in toggles all the countries
		Uint8 color = 13 + 8 * (offset % GRAPH_MAX_BUTTONS);
		_countryToggles.push_back(new GraphButInfo("rule/" + country->getRules()->getType(), tr(country->getRules()->getType()), color));
		// initially add the GRAPH_MAX_BUTTONS having the first countries information
		if (offset < GRAPH_MAX_BUTTONS)
		{
			_btnCountries.push_back(new ToggleTextButton(88, 11, 0, offset*11));
			_btnCountries.at(offset)->setInvertColor(color);
			_btnCountries.at(offset)->setText(tr(country->getRules()->getType()));
			_btnCountries.at(offset)->onMousePress((ActionHandler)&GraphsState::btnCountryListClick);
			add(_btnCountries.at(offset), "button", "graphs");
		}
		_alienCountryLines.push_back(new Surface(320,200,0,0));
		add(_alienCountryLines.at(offset));
		_xcomCountryLines.push_back(new Surface(320,200,0,0));
		add(_xcomCountryLines.at(offset));
		_incomeLines.push_back(new Surface(320,200,0,0));
		add(_incomeLines.at(offset));

		++offset;
	}

	if (_countryToggles.size() < GRAPH_MAX_BUTTONS)
		_btnCountryTotal = new ToggleTextButton(88, 11, 0, _countryToggles.size()*11);
	else
		_btnCountryTotal = new ToggleTextButton(88, 11, 0, GRAPH_MAX_BUTTONS*11);
	_countryToggles.push_back(new GraphButInfo("total", tr("STR_TOTAL_UC"), countryTotalColor));
	_btnCountryTotal->onMousePress((ActionHandler)&GraphsState::btnCountryListClick);
	_btnCountryTotal->setInvertColor(countryTotalColor);
	_btnCountryTotal->setText(tr("STR_TOTAL_UC"));
	_alienCountryLines.push_back(new Surface(320,200,0,0));
	add(_alienCountryLines.at(offset));
	_xcomCountryLines.push_back(new Surface(320,200,0,0));
	add(_xcomCountryLines.at(offset));
	_incomeLines.push_back(new Surface(320,200,0,0));
	add(_incomeLines.at(offset));
	add(_btnCountryTotal, "button", "graphs");


	for (int iter = 0; iter != 5; ++iter)
	{
		offset = iter;
		_btnFinances.push_back(new ToggleTextButton(88, 11, 0, offset*11));
		_financeToggles.push_back(false);
		_btnFinances.at(offset)->setInvertColor(13 + (8*offset));
		_btnFinances.at(offset)->onMousePress((ActionHandler)&GraphsState::btnFinanceListClick);
		add(_btnFinances.at(offset), "button", "graphs");
		_financeLines.push_back(new Surface(320,200,0,0));
		add(_financeLines.at(offset));
	}

	_btnFinances.at(0)->setText(tr("STR_INCOME"));
	_btnFinances.at(1)->setText(tr("STR_EXPENDITURE"));
	_btnFinances.at(2)->setText(tr("STR_MAINTENANCE"));
	_btnFinances.at(3)->setText(tr("STR_BALANCE"));
	_btnFinances.at(4)->setText(tr("STR_SCORE"));

	// load back the button state
	std::string graphRegionToggles = _game->getSavedGame()->getGraphRegionToggles();
	std::string graphCountryToggles = _game->getSavedGame()->getGraphCountryToggles();
	std::string graphFinanceToggles = _game->getSavedGame()->getGraphFinanceToggles();
	while (graphRegionToggles.size() < _regionToggles.size()) graphRegionToggles.push_back('0');
	while (graphCountryToggles.size() < _countryToggles.size()) graphCountryToggles.push_back('0');
	while (graphFinanceToggles.size() < _financeToggles.size()) graphFinanceToggles.push_back('0');
	const auto &preferences = _game->getSavedGame()->getGraphVisibility();
	auto visible = [&](const std::string &scope, const std::string &id, bool legacy) {
		if (preferences.empty()) return legacy;
		auto it = preferences.find(scope + "/" + id);
		return it != preferences.end() && it->second;
	};
	for (size_t i = 0; i < _regionToggles.size(); ++i)
	{
		_regionToggles[i]->_pushed = visible("region", _regionToggles[i]->_id, graphRegionToggles[i] != '0');
		if (_regionToggles.size()-1 == i)
			_btnRegionTotal->setPressed(_regionToggles[i]->_pushed);
		else if (i < GRAPH_MAX_BUTTONS)
			_btnRegions.at(i)->setPressed(_regionToggles[i]->_pushed);
	}
	for (size_t i = 0; i < _countryToggles.size(); ++i)
	{
		_countryToggles[i]->_pushed = visible("country", _countryToggles[i]->_id, graphCountryToggles[i] != '0');
		if (_countryToggles.size()-1 == i)
			_btnCountryTotal->setPressed(_countryToggles[i]->_pushed);
		else if (i < GRAPH_MAX_BUTTONS)
			_btnCountries.at(i)->setPressed(_countryToggles[i]->_pushed);
	}
	for (size_t i = 0; i < _financeToggles.size(); ++i)
	{
		_financeToggles[i] = ('0'==graphFinanceToggles[i]) ? false : true;
		_btnFinances.at(i)->setPressed(_financeToggles[i]);
	}
	Uint8 gridColor = _game->getMod()->getInterface("graphs")->getElement("graph")->color;
	// set up the grid
	_bg->drawRect(125, 49, 188, 127, gridColor);

	for (int grid = 0; grid !=5; ++grid)
	{
		for (int y = 50 + grid; y <= 163 + grid; y += 14)
		{
			for (int x = 126 + grid; x <= 297 + grid; x += 17)
			{
				Uint8 color = gridColor + grid + 1;
				if (grid == 4)
				{
					color = 0;
				}
				_bg->drawRect(x, y, 16 - (grid*2), 13 - (grid*2), color);
			}
		}
	}

	//set up the horizontal measurement unit
	std::string months[] = {"STR_JAN", "STR_FEB", "STR_MAR", "STR_APR", "STR_MAY", "STR_JUN", "STR_JUL", "STR_AUG", "STR_SEP", "STR_OCT", "STR_NOV", "STR_DEC"};
	int month = _game->getSavedGame()->getTime()->getMonth();
	// i know using textlist for this is ugly and brutal, but YOU try getting this damn text to line up.
	// also, there's nothing wrong with being ugly or brutal, you should learn tolerance.
	_txtMonths->setColumns(12, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17);
	_txtMonths->addRow(12, " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ");
	_txtYears->setColumns(6, 34, 34, 34, 34, 34, 34);
	_txtYears->addRow(6, " ", " ", " ", " ", " ", " ");

	for (int iter = 0; iter != 12; ++iter)
	{
		if (month > 11)
		{
			month = 0;
			std::ostringstream ss;
			ss << _game->getSavedGame()->getTime()->getYear();
			_txtYears->setCellText(0, iter/2, ss.str());
			if (iter > 2)
			{
				std::ostringstream ss2;
				ss2 << (_game->getSavedGame()->getTime()->getYear()-1);
				_txtYears->setCellText(0, 0, ss2.str());
			}
		}
		_txtMonths->setCellText(0, iter, tr(months[month]));
		++month;
	}

	// set up the vertical measurement unit
	for (auto* scaleText : _txtScale)
	{
		scaleText->setAlign(ALIGN_RIGHT);
	}
	btnUfoRegionClick(0);

	// Set up object
#ifndef __EMSCRIPTEN__
	if (_game->getMod()->getSurface("GRAPH.BDY", false))
	{
		_game->getMod()->getSurface("GRAPH.BDY")->blitNShade(_bg, 0, 0);
	}
	else
	{
		_game->getMod()->getSurface("GRAPHS.SPK")->blitNShade(_bg, 0, 0);
	}

#endif
	_txtTitle->setAlign(ALIGN_CENTER);

	_txtFactor->setText(tr("STR_FINANCE_THOUSANDS"));

	// Set up buttons
	_btnUfoRegion->onMousePress((ActionHandler)&GraphsState::btnUfoRegionClick);
	_btnUfoCountry->onMousePress((ActionHandler)&GraphsState::btnUfoCountryClick);
	_btnXcomRegion->onMousePress((ActionHandler)&GraphsState::btnXcomRegionClick);
	_btnXcomCountry->onMousePress((ActionHandler)&GraphsState::btnXcomCountryClick);
	_btnIncome->onMousePress((ActionHandler)&GraphsState::btnIncomeClick);
	_btnFinance->onMousePress((ActionHandler)&GraphsState::btnFinanceClick);
	_btnGeoscape->onMousePress((ActionHandler)&GraphsState::btnGeoscapeClick);
	_btnGeoscape->onKeyboardPress((ActionHandler)&GraphsState::btnGeoscapeClick, Options::keyCancel);
	_btnGeoscape->onKeyboardPress((ActionHandler)&GraphsState::btnGeoscapeClick, Options::keyGeoGraphs);
	_btnGeoscape->onKeyboardPress((ActionHandler)&GraphsState::btnZoomInClick, Options::keyGraphsZoomIn);
	_btnGeoscape->onKeyboardPress((ActionHandler)&GraphsState::btnZoomOutClick, Options::keyGraphsZoomOut);

#ifdef __EMSCRIPTEN__
	_hd = new Calypso::CalypsoStrategicGraphs(*this);
#else
	centerAllSurfaces();
#endif
}

/**
 *
 */
#ifdef __EMSCRIPTEN__
void GraphsState::hdSetVisible(size_t index, bool visible)
{
	if (_finance) _financeToggles.at(index) = visible;
	else (_country ? _countryToggles : _regionToggles).at(index)->_pushed = visible;
}
std::string GraphsState::hdName(size_t index) const
{
	if (_finance) return _btnFinances.at(index)->getText();
	return (_country ? _countryToggles : _regionToggles).at(index)->_name;
}
void GraphsState::hdClick(Action *action) { _hd->click(action); }
void GraphsState::resize(int &dX, int &dY)
{
	State::resize(dX, dY);
	if (_hd) _hd->layout();
}
#endif

GraphsState::~GraphsState()
{
#ifdef __EMSCRIPTEN__
	delete _hd;
#endif
	std::string graphRegionToggles;
	std::string graphCountryToggles;
	std::string graphFinanceToggles;
	for (size_t i = 0; i < _regionToggles.size(); ++i)
	{
		graphRegionToggles.push_back(_regionToggles[i]->_pushed ? '1' : '0');
		_game->getSavedGame()->getGraphVisibility()["region/" + _regionToggles[i]->_id] = _regionToggles[i]->_pushed;
		delete _regionToggles[i];
	}
	for (size_t i = 0; i < _countryToggles.size(); ++i)
	{
		graphCountryToggles.push_back(_countryToggles[i]->_pushed ? '1' : '0');
		_game->getSavedGame()->getGraphVisibility()["country/" + _countryToggles[i]->_id] = _countryToggles[i]->_pushed;
		delete _countryToggles[i];
	}
	for (size_t i = 0; i < _financeToggles.size(); ++i)
	{
		graphFinanceToggles.push_back(_financeToggles[i] ? '1' : '0');
	}
	_game->getSavedGame()->setGraphRegionToggles(graphRegionToggles);
	_game->getSavedGame()->setGraphCountryToggles(graphCountryToggles);
	_game->getSavedGame()->setGraphFinanceToggles(graphFinanceToggles);
}

/**
* Zooms in.
* @param action Pointer to an action.
*/
void GraphsState::btnZoomInClick(Action *)
{
	_zoom = _zoom * 2 / 3;
	if (_zoom < 5) _zoom = 5;

	drawLines();
}

/**
* Zooms out.
* @param action Pointer to an action.
*/
void GraphsState::btnZoomOutClick(Action *)
{
	_zoom = _zoom * 3 / 2;
	if (_zoom > 100) _zoom = 100;

	drawLines();
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void GraphsState::btnGeoscapeClick(Action *)
{
	_game->popState();
}

/**
 * Switches to the UFO Region Activity screen.
 * @param action Pointer to an action.
 */
void GraphsState::btnUfoRegionClick(Action *)
{
	_alien = true;
	_income = false;
	_country = false;
	_finance = false;
	resetScreen();
	drawLines();
	for (auto* toggleButton : _btnRegions)
	{
		toggleButton->setVisible(true);
	}
	_btnRegionTotal->setVisible(true);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_UFO_ACTIVITY_IN_AREAS"));
}

/**
 * Switches to the UFO Country activity screen.
 * @param action Pointer to an action.
 */
void GraphsState::btnUfoCountryClick(Action *)
{
	_alien = true;
	_income = false;
	_country = true;
	_finance = false;
	resetScreen();
	drawLines();
	for (auto* toggleButton : _btnCountries)
	{
		toggleButton->setVisible(true);
	}
	_btnCountryTotal->setVisible(true);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_UFO_ACTIVITY_IN_COUNTRIES"));
}

/**
 * Switches to the XCom Region activity screen.
 * @param action Pointer to an action.
 */
void GraphsState::btnXcomRegionClick(Action *)
{
	_alien = false;
	_income = false;
	_country = false;
	_finance = false;
	resetScreen();
	drawLines();
	for (auto* toggleButton : _btnRegions)
	{
		toggleButton->setVisible(true);
	}
	_btnRegionTotal->setVisible(true);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_XCOM_ACTIVITY_IN_AREAS"));
}

/**
 * Switches to the XCom Country activity screen.
 * @param action Pointer to an action.
 */
void GraphsState::btnXcomCountryClick(Action *)
{
	_alien = false;
	_income = false;
	_country = true;
	_finance = false;
	resetScreen();
	drawLines();
	for (auto* toggleButton : _btnCountries)
	{
		toggleButton->setVisible(true);
	}
	_btnCountryTotal->setVisible(true);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_XCOM_ACTIVITY_IN_COUNTRIES"));
}

/**
 * Switches to the Income screen.
 * @param action Pointer to an action.
 */
void GraphsState::btnIncomeClick(Action *)
{
	_alien = false;
	_income = true;
	_country = true;
	_finance = false;
	resetScreen();
	drawLines();
	_txtFactor->setVisible(true);
	for (auto* toggleButton : _btnCountries)
	{
		toggleButton->setVisible(true);
	}
	_btnCountryTotal->setVisible(true);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_INCOME"));
}

/**
 * Switches to the Finances screen.
 * @param action Pointer to an action.
 */
void GraphsState::btnFinanceClick(Action *)
{
	_alien = false;
	_income = false;
	_country = false;
	_finance = true;
	resetScreen();
	drawLines();

	for (auto* toggleButton : _btnFinances)
	{
		toggleButton->setVisible(true);
	}
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_FINANCE"));

}

/**
 * Handles a click on a region button.
 * @param action Pointer to an action.
 */
void GraphsState::btnRegionListClick(Action * action)
{
	size_t number = 0;
	ToggleTextButton *button = dynamic_cast<ToggleTextButton*>(action->getSender());

	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		for (size_t i = 0; i < _btnRegions.size(); ++i)
		{
			if (button != _btnRegions[i])
			{
				_btnRegions[i]->setPressed(button->getPressed());
			}
		}
		_btnRegionTotal->setPressed(button->getPressed());
		for (auto* gbi : _regionToggles)
		{
			gbi->_pushed = button->getPressed();
		}
	}
	else
	{
		if (button == _btnRegionTotal)
		{
			number = _regionToggles.size() - 1;
		}
		else
		{
			for (size_t i = 0; i < _btnRegions.size(); ++i)
			{
				if (button == _btnRegions[i])
				{
					number = i + _butRegionsOffset;
					break;
				}
			}
		}

		_regionToggles.at(number)->_pushed = button->getPressed();
	}

	drawLines();
}

/**
 * Handles a click on a country button.
 * @param action Pointer to an action.
 */
void GraphsState::btnCountryListClick(Action * action)
{
	size_t number = 0;
	ToggleTextButton *button = dynamic_cast<ToggleTextButton*>(action->getSender());

	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		for (size_t i = 0; i < _btnCountries.size(); ++i)
		{
			if (button != _btnCountries[i])
			{
				_btnCountries[i]->setPressed(button->getPressed());
			}
		}
		_btnCountryTotal->setPressed(button->getPressed());
		for (auto* gbi : _countryToggles)
		{
			gbi->_pushed = button->getPressed();
		}
	}
	else
	{
		if (button == _btnCountryTotal)
		{
			number = _countryToggles.size() - 1;
		}
		else
		{
			for (size_t i = 0; i < _btnCountries.size(); ++i)
			{
				if (button == _btnCountries[i])
				{
					number = i + _butCountriesOffset;
					break;
				}
			}
		}

		_countryToggles.at(number)->_pushed = button->getPressed();
	}

	drawLines();
}

/**
 * handles a click on a finances button.
 * @param action Pointer to an action.
 */
void GraphsState::btnFinanceListClick(Action *action)
{
	size_t number = 0;
	ToggleTextButton *button = dynamic_cast<ToggleTextButton*>(action->getSender());

	for (size_t i = 0; i < _btnFinances.size(); ++i)
	{
		if (button == _btnFinances[i])
		{
			number = i;
			break;
		}
	}

	_financeLines.at(number)->setVisible(!_financeToggles.at(number));
	_financeToggles.at(number) = button->getPressed();

	drawLines();
}

/**
 * remove all elements from view
 */
void GraphsState::resetScreen()
{
	for (auto* surface : _alienRegionLines)
	{
		surface->setVisible(false);
	}
	for (auto* surface : _alienCountryLines)
	{
		surface->setVisible(false);
	}
	for (auto* surface : _xcomRegionLines)
	{
		surface->setVisible(false);
	}
	for (auto* surface : _xcomCountryLines)
	{
		surface->setVisible(false);
	}
	for (auto* surface : _incomeLines)
	{
		surface->setVisible(false);
	}
	for (auto* surface : _financeLines)
	{
		surface->setVisible(false);
	}


	for (auto* ttb : _btnRegions)
	{
		ttb->setVisible(false);
	}
	for (auto* ttb : _btnCountries)
	{
		ttb->setVisible(false);
	}
	for (auto* ttb : _btnFinances)
	{
		ttb->setVisible(false);
	}

	_btnRegionTotal->setVisible(false);
	_btnCountryTotal->setVisible(false);
	_txtFactor->setVisible(false);
}

/**
 * updates the text on the vertical scale
 * @param lowerLimit minimum value
 * @param upperLimit maximum value
 */
void GraphsState::updateScale(double lowerLimit, double upperLimit)
{
	double increment = ((upperLimit - lowerLimit) / 9);
	double text = lowerLimit;
	for (int i = 0; i < 10; ++i)
	{
		_txtScale.at(i)->setText(Unicode::formatNumber(static_cast<int64_t>(std::llround(text))));
		text += increment;
	}
}

/**
 * instead of having all our line drawing in one giant ridiculous routine, just use the one we need.
 */
void GraphsState::drawLines()
{
	auto *save = _game->getSavedGame();
	const auto &surfaces = _finance ? _financeLines : _income ? _incomeLines :
		_country ? (_alien ? _alienCountryLines : _xcomCountryLines) : (_alien ? _alienRegionLines : _xcomRegionLines);
	_series.assign(surfaces.size(), {});
	if (_finance)
	{
		for (size_t i = 0; i < _series.size(); ++i)
		{
			_series[i].color = Palette::blockOffset(i / 2 + 1) + (i % 2 ? 8 : 0);
			_series[i].visible = _financeToggles[i];
		}
		for (size_t month = 0; month < StrategicGraphMonths; ++month)
		{
			_series[0].values[month] = strategicGraphValue(save->getIncomes(), month) / 1000;
			_series[1].values[month] = strategicGraphValue(save->getExpenditures(), month) / 1000;
			_series[2].values[month] = strategicGraphValue(save->getMaintenances(), month) / 1000;
			_series[3].values[month] = strategicGraphValue(save->getFundsList(), month) / 1000;
			double score = strategicGraphValue(save->getResearchScores(), month);
			for (auto *region : *save->getRegions())
			{
				score += strategicGraphValue(region->getActivityXcom(), month);
				score -= strategicGraphValue(region->getActivityAlien(), month);
			}
			_series[4].values[month] = score;
		}
		_series[2].values[0] += save->getBaseMaintenance() / 1000;
	}
	else
	{
		const auto &toggles = _country ? _countryToggles : _regionToggles;
		for (size_t i = 0; i < toggles.size(); ++i)
		{
			_series[i].visible = toggles[i]->_pushed;
			_series[i].color = toggles[i]->_color + 4;
		}
		_series.back().color = _game->getMod()->getInterface("graphs")
			->getElement(_country ? "countryTotal" : "regionTotal")->color2;
		for (size_t i = 0; i + 1 < _series.size(); ++i)
		{
			const std::vector<int> *history;
			if (_country)
			{
				auto *country = save->getCountries()->at(i);
				history = _income ? &country->getFunding() :
					_alien ? &country->getActivityAlien() : &country->getActivityXcom();
			}
			else
			{
				auto *region = save->getRegions()->at(i);
				history = _alien ? &region->getActivityAlien() : &region->getActivityXcom();
			}
			for (size_t month = 0; month < StrategicGraphMonths; ++month)
			{
				const auto value = strategicGraphValue(*history, month) / (_income ? 1000 : 1);
				_series[i].values[month] = value;
				_series.back().values[month] += value;
			}
		}
	}
	double low = 0, high = 0;
	for (const auto &line : _series)
	{
		if (!line.visible) continue;
		auto range = std::minmax_element(line.values.begin(), line.values.end());
		low = std::min(low, *range.first);
		high = std::max(high, *range.second);
	}
	_axis = strategicGraphAxis(low, high, _zoom, _finance ? 250 : _income ? 50 : 10);
#ifdef __EMSCRIPTEN__
	++_hdRevision;
	return;
#endif
	for (size_t i = 0; i < _series.size(); ++i)
	{
		auto *surface = surfaces[i];
		const auto &line = _series[i];
		surface->clear();
		surface->setVisible(line.visible);
		if (!line.visible) continue;
		int previousY = 0;
		for (size_t month = 0; month < StrategicGraphMonths; ++month)
		{
			const int x = 312 - static_cast<int>(month) * 17;
			const int y = 175 - static_cast<int>(std::lround(126 * _axis.fraction(line.values[month])));
			if (month) surface->drawLine(x, y, x + 17, previousY, line.color);
			previousY = y;
		}
	}
	updateScale(_axis.low, _axis.high);
	_txtFactor->setVisible(_income || _finance);
}

/**
 * 'Shift' the buttons to display only GRAPH_MAX_BUTTONS - reset their state from toggle
 */
void GraphsState::shiftButtons(Action *action)
{
	// only if active 'screen' is other than finance
	if (_finance)
		return;
	// select the data's we'll processing - regions or countries
	if (_country)
	{
		// too few countries? - return
		if (_countryToggles.size() <= GRAPH_MAX_BUTTONS)
			return;
		else if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP)
			scrollButtons(_countryToggles, _btnCountries, _butCountriesOffset, -1);
		else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
			scrollButtons(_countryToggles, _btnCountries, _butCountriesOffset, 1);
	}
	else
	{
		// too few regions? - return
		if (_regionToggles.size() <= GRAPH_MAX_BUTTONS)
			return;
		else if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP)
			scrollButtons(_regionToggles, _btnRegions, _butRegionsOffset, -1);
		else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
			scrollButtons(_regionToggles, _btnRegions, _butRegionsOffset, 1);
	}
}

void GraphsState::scrollButtons(std::vector<GraphButInfo *> &toggles, std::vector<ToggleTextButton *> &buttons, size_t &offset, int step)
{
	if ( int(step + (int)offset) < 0 || offset + step + GRAPH_MAX_BUTTONS >= toggles.size())
		return;
	// set the next offset - cheaper to do it from starter
	offset += step;
	size_t i = 0;
	std::vector<ToggleTextButton *>::iterator iterb = buttons.begin();
	for (std::vector<GraphButInfo *>::iterator itert = toggles.begin() + offset; itert != toggles.end() && i < GRAPH_MAX_BUTTONS; ++itert, ++iterb, ++i)
	{
		updateButton(*itert, *iterb);
	}
}

void GraphsState::updateButton(GraphButInfo *from,ToggleTextButton *to)
{
	to->setText(from->_name);
	to->setInvertColor(from->_color);
	to->setPressed(from->_pushed);
}

}
