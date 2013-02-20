/*
* Copyright (C) 2009-2012 Rajko Stojadinovic <http://github.com/rajkosto/hive>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "SqlCharDataSource.h"
#include "Database/Database.h"

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;
using boost::bad_lexical_cast;
#include <boost/algorithm/string/predicate.hpp>

SqlCharDataSource::SqlCharDataSource( Poco::Logger& logger, shared_ptr<Database> db, const string& idFieldName, const string& wsFieldName ) : SqlDataSource(logger,db)
{
	_idFieldName = getDB()->escape(idFieldName);
	_wsFieldName = getDB()->escape(wsFieldName);
}

SqlCharDataSource::~SqlCharDataSource() {}

Sqf::Value SqlCharDataSource::fetchCharacterInitial( string playerId, int serverId, const string& playerName )
{
	Sqf::Parameters retVal;
	
	auto playerResPtr(getDB()->namedQueryParams("CALL LoadCharacter(%d,0,'%s','%s')",serverId,getDB()->escape(playerId).c_str(),getDB()->escape(playerName).c_str()));
	if (playerResPtr && playerResPtr->fetchRow())
	{
		auto& res = *playerResPtr;

		const string playerStatus = res["PlayerStatus"].getString();
		{
			
			if (playerStatus == "ERROR")
			{
				_logger.error("Unable to set/fetch player info for UID '"+playerId+"' Name: '"+playerName+"'");
				retVal.push_back(string("ERROR"));
				retVal.push_back(string("Player"));
				return retVal;
			}
			else if (playerStatus == "NEW")
				_logger.information("Created a new player " + playerId + " named '"+playerName+"'");
			else if (playerStatus == "CHNAME")
				_logger.information("Changed name of player " + playerId + " from '"+res["OldName"].getString()+"' to '"+playerName+"'");
			else if (playerStatus == "EXIST") {}
			else
				_logger.warning("Received unknown PlayerStatus '"+playerStatus+"' for UID '"+playerId+"' Name: '"+playerName+"'");
		}

		const string charStatus = res["CharacterStatus"].getString();
		{			
			if (charStatus == "ERROR")
			{
				_logger.error("Unable to create/fetch character info for UID '"+playerId+"' Name: '"+playerName+"'");
				retVal.push_back(string("ERROR"));
				retVal.push_back(string("Character"));
				return retVal;
			}
			else if (charStatus == "NEW1ST" || charStatus == "NEWFROM")
			{
				string logStr = "Created a new character " + res["CharacterID"].getString() + " for player '"+playerName+"' (" + playerId + ")";
				if (charStatus == "NEW1ST")
					logStr += " from scratch";
				else
					logStr += " from existing character";

				_logger.information(logStr);
			}
			else if (charStatus == "EXIST") {}
			else
				_logger.warning("Received unknown CharacterStatus '"+charStatus+"' for UID '"+playerId+"' Name: '"+playerName+"'");
		}

		bool newPlayer = boost::icontains(playerStatus,"new");
		bool newChar = boost::icontains(charStatus,"new");
		UInt32 charId = res["CharacterID"].getUInt32();

		if (res.nextResult() && res.fetchRow())
		{
			Sqf::Value worldSpace = Sqf::Parameters(); //empty worldspace
			Sqf::Value inventory = Sqf::Parameters(); //empty inventory
			Sqf::Value backpack = Sqf::Parameters(); //empty backpack
			Sqf::Value survival = lexical_cast<Sqf::Value>("[0,0,0]"); //0 mins alive, 0 mins since last ate, 0 mins since last drank
			if (!newChar)
			{
				if (!res["Worldspace"].isNull())
				{
					const string wsStr = res["Worldspace"].getString();
					try { worldSpace = lexical_cast<Sqf::Value>(wsStr); }
					catch(const bad_lexical_cast&)
					{
						_logger.warning("Invalid Worldspace for CharacterID("+lexical_cast<string>(charId)+"): "+wsStr);
					}
				}

				if (!res["Inventory"].isNull()) //inventory can be null
				{
					const string invStr = res["Inventory"].getString();
					try
					{
						inventory = lexical_cast<Sqf::Value>(invStr);
						try { SanitiseInv(boost::get<Sqf::Parameters>(inventory)); } catch (const boost::bad_get&) {}
					}
					catch(const bad_lexical_cast&)
					{
						_logger.warning("Invalid Inventory for CharacterID("+lexical_cast<string>(charId)+"): "+invStr);
					}
				}	

				if (!res["Backpack"].isNull()) //backpack can be null
				{
					const string backStr = res["Backpack"].getString();
					try { backpack = lexical_cast<Sqf::Value>(backStr); }
					catch(bad_lexical_cast)
					{
						_logger.warning("Invalid Backpack for CharacterID("+lexical_cast<string>(charId)+"): "+backStr);
					}
				}

				//set survival info
				{
					auto& survivalArr = boost::get<Sqf::Parameters>(survival);
					survivalArr[0] = res["SurvivalTime"].getInt32();
					survivalArr[1] = res["MinsLastAte"].getInt32();
					survivalArr[2] = res["MinsLastDrank"].getInt32();
				}
			}			

			string model = ""; //empty models will be defaulted by scripts
			try
			{
				//remove quotes from string and such, if present
				model = boost::get<string>(lexical_cast<Sqf::Value>(res["Model"].getString()));
			}
			catch(...)
			{
				model = res["Model"].getString();
			}

			retVal.push_back(string("PASS"));
			retVal.push_back(newPlayer);
			retVal.push_back(lexical_cast<string>(charId));
			if (!newChar)
			{
				retVal.push_back(worldSpace);
				retVal.push_back(inventory);
				retVal.push_back(backpack);
				retVal.push_back(survival);
			}
			retVal.push_back(model);
			//hive interface version
			retVal.push_back(0.96f);
			return retVal;
		}
	}

	//if we got here, that means we failed to fetch some result or row
	retVal.push_back(string("ERROR"));
	retVal.push_back(string("Database"));
	return retVal;
}

Sqf::Value SqlCharDataSource::fetchCharacterDetails( int characterId )
{
	Sqf::Parameters retVal;
	//get details from db
	auto charDetRes = getDB()->queryParams(
		"SELECT `%s`, `Medical`, `Generation`, `KillsZ`, `HeadshotsZ`, `KillsH`, `KillsB`, `CurrentState`, `Humanity` "
		"FROM `Character_DATA` WHERE `CharacterID`=%d", _wsFieldName.c_str(), characterId);

	if (charDetRes && charDetRes->fetchRow())
	{
		Sqf::Value worldSpace = Sqf::Parameters(); //empty worldspace
		Sqf::Value medical = Sqf::Parameters(); //script will fill this in if empty
		int generation = 1;
		Sqf::Value stats = lexical_cast<Sqf::Value>("[0,0,0,0]"); //killsZ, headZ, killsH, killsB
		Sqf::Value currentState = Sqf::Parameters(); //empty state (aiming, etc)
		int humanity = 2500;
		//get stuff from row
		{
			try
			{
				worldSpace = lexical_cast<Sqf::Value>(charDetRes->at(0).getString());
			}
			catch(bad_lexical_cast)
			{
				_logger.warning("Invalid Worldspace (detail load) for CharacterID("+lexical_cast<string>(characterId)+"): "+charDetRes->at(0).getString());
			}
			try
			{
				medical = lexical_cast<Sqf::Value>(charDetRes->at(1).getString());
			}
			catch(bad_lexical_cast)
			{
				_logger.warning("Invalid Medical (detail load) for CharacterID("+lexical_cast<string>(characterId)+"): "+charDetRes->at(1).getString());
			}
			generation = charDetRes->at(2).getInt32();
			//set stats
			{
				Sqf::Parameters& statsArr = boost::get<Sqf::Parameters>(stats);
				statsArr[0] = charDetRes->at(3).getInt32();
				statsArr[1] = charDetRes->at(4).getInt32();
				statsArr[2] = charDetRes->at(5).getInt32();
				statsArr[3] = charDetRes->at(6).getInt32();
			}
			try
			{
				currentState = lexical_cast<Sqf::Value>(charDetRes->at(7).getString());
			}
			catch(bad_lexical_cast)
			{
				_logger.warning("Invalid CurrentState (detail load) for CharacterID("+lexical_cast<string>(characterId)+"): "+charDetRes->at(7).getString());
			}
			humanity = charDetRes->at(8).getInt32();
		}

		retVal.push_back(string("PASS"));
		retVal.push_back(medical);
		retVal.push_back(stats);
		retVal.push_back(currentState);
		retVal.push_back(worldSpace);
		retVal.push_back(humanity);
	}
	else
	{
		retVal.push_back(string("ERROR"));
	}

	return retVal;
}

bool SqlCharDataSource::updateCharacter( int characterId, const FieldsType& fields )
{
	map<string,string> sqlFields;

	for (auto it=fields.begin();it!=fields.end();++it)
	{
		const string& name = it->first;
		const Sqf::Value& val = it->second;

		//arrays
		if (name == "Worldspace" || name == "Inventory" || name == "Backpack" || name == "Medical" || name == "CurrentState")
			sqlFields[name] = "'"+getDB()->escape(lexical_cast<string>(val))+"'";
		//booleans
		else if (name == "JustAte" || name == "JustDrank")
		{
			if (boost::get<bool>(val))
			{
				string newName = "LastAte";
				if (name == "JustDrank")
					newName = "LastDrank";

				sqlFields[newName] = "CURRENT_TIMESTAMP";
			}
		}
		//addition integeroids
		else if (name == "KillsZ" || name == "HeadshotsZ" || name == "DistanceFoot" || name == "Duration" ||
			name == "KillsH" || name == "KillsB" || name == "Humanity")
		{
			int integeroid = static_cast<int>(Sqf::GetDouble(val));
			char intSign = '+';
			if (integeroid < 0)
			{
				intSign = '-';
				integeroid = abs(integeroid);
			}

			if (integeroid > 0) 
				sqlFields[name] = "(`"+name+"` "+intSign+" "+lexical_cast<string>(integeroid)+")";
		}
		//strings
		else if (name == "Model")
			sqlFields[name] = "'"+getDB()->escape(boost::get<string>(val))+"'";
	}

	if (sqlFields.size() > 0)
	{
		string query = "UPDATE `Character_DATA` SET ";
		for (auto it=sqlFields.begin();it!=sqlFields.end();)
		{
			string fieldName = it->first;
			if (fieldName == "Worldspace")
				fieldName = _wsFieldName;

			query += "`" + fieldName + "` = " + it->second;
			++it;
			if (it != sqlFields.end())
				query += " , ";
		}
		query += " WHERE `CharacterID` = " + lexical_cast<string>(characterId);
		bool exRes = getDB()->execute(query.c_str());
		poco_assert(exRes == true);

		return exRes;
	}

	return true;
}

bool SqlCharDataSource::initCharacter( int characterId, const Sqf::Value& inventory, const Sqf::Value& backpack )
{
	auto stmt = getDB()->makeStatement(_stmtInitCharacter, "UPDATE `Character_DATA` SET `Inventory` = ? , `Backpack` = ? WHERE `CharacterID` = ?");
	stmt->addString(lexical_cast<string>(inventory));
	stmt->addString(lexical_cast<string>(backpack));
	stmt->addInt32(characterId);
	bool exRes = stmt->execute();
	poco_assert(exRes == true);

	return exRes;
}

bool SqlCharDataSource::killCharacter( int characterId, int duration )
{
	auto stmt = getDB()->makeStatement(_stmtKillCharacter, 
		"UPDATE `Character_DATA` SET `Alive` = 0, `LastLogin` = DATE_SUB(CURRENT_TIMESTAMP, INTERVAL ? MINUTE) WHERE `CharacterID` = ? AND `Alive` = 1");
	stmt->addInt32(duration);
	stmt->addInt32(characterId);
	bool exRes = stmt->execute();
	poco_assert(exRes == true);

	return exRes;
}

bool SqlCharDataSource::recordLogin( string playerId, int characterId, int action )
{	
	auto stmt = getDB()->makeStatement(_stmtRecordLogin, 
		"INSERT INTO `Player_LOGIN` (`"+_idFieldName+"`, `CharacterID`, `Datestamp`, `Action`) VALUES (?, ?, CURRENT_TIMESTAMP, ?)");
	stmt->addString(playerId);
	stmt->addInt32(characterId);
	stmt->addInt32(action);
	bool exRes = stmt->execute();
	poco_assert(exRes == true);

	return exRes;
}