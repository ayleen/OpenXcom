/*
 * Copyright 2010-2019 OpenXcom Developers.
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
#include "GeoscapeEvent.h"
#include <assert.h>
#include "../Mod/RuleEvent.h"

namespace OpenXcom
{

GeoscapeEvent::GeoscapeEvent(const RuleEvent &rule) : _rule(rule), _spawnCountdown(0), _over(false)
{
	// Empty by design.
}

GeoscapeEvent::~GeoscapeEvent()
{
	// Empty by design.
}

/**
 * Loads the event from YAML.
 * @param node The YAML node containing the data.
 */
void GeoscapeEvent::load(const YAML::YamlNodeReader& reader)
{
	reader.tryRead("spawnCountdown", _spawnCountdown);
	reader.tryRead("over", _over);
	const auto notice = reader["notice"];
	if (notice)
	{
		notice.tryRead("order", _receipt.order);
		notice.tryRead("applied", _receipt.applied);
		notice.tryRead("acknowledged", _receipt.acknowledged);
		notice.tryRead("region", _receipt.region);
		notice.tryRead("city", _receipt.city);
		notice.tryRead("research", _receipt.research);
		notice.tryRead("bonusResearch", _receipt.bonusResearch);
		notice.tryRead("recoveryFile", _receipt.recoveryFile);
		_receipt.items = notice["items"].readVal(_receipt.items);
	}
}

/**
 * Saves the event to YAML.
 * @return YAML node.
 */
void GeoscapeEvent::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("name", _rule.getName());
	writer.write("spawnCountdown", _spawnCountdown);
	if (_over)
		writer.write("over", _over);
	if (_receipt.order || _receipt.acknowledged)
	{
		auto note = writer["notice"];
		note.setAsMap();
		note.write("order", _receipt.order);
		note.write("applied", _receipt.applied);
		note.write("acknowledged", _receipt.acknowledged);
		if (!_receipt.region.empty()) note.write("region", _receipt.region);
		if (_receipt.city >= 0) note.write("city", _receipt.city);
		if (!_receipt.items.empty()) note.write("items", _receipt.items);
		if (!_receipt.research.empty()) note.write("research", _receipt.research);
		if (!_receipt.bonusResearch.empty()) note.write("bonusResearch", _receipt.bonusResearch);
		if (!_receipt.recoveryFile.empty()) note.write("recoveryFile", _receipt.recoveryFile);
	}
}

/**
 * The new time must be a multiple of 30 minutes, and more than 0.
 * Calling this on a finished event has no effect.
 * @param minutes The minutes until the event will pop up.
 */
void GeoscapeEvent::setSpawnCountdown(size_t minutes)
{
	assert(minutes != 0 && minutes % 30 == 0);
	if (_over)
	{
		return;
	}
	_spawnCountdown = minutes;
}

void GeoscapeEvent::think()
{
	// if finished, don't do anything
	if (_over)
	{
		return;
	}

	// are we there yet?
	if (_spawnCountdown > 30)
	{
		_spawnCountdown -= 30;
		return;
	}

	// ok, the time has come...
	_over = true;
}

}
