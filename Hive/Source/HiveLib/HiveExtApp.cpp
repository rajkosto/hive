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

#include "HiveExtApp.h"

#include <boost/bind.hpp>
#include <boost/optional.hpp>

#include <boost/algorithm/string/predicate.hpp>

void HiveExtApp::setupClock()
{
	namespace pt = boost::posix_time;
	pt::ptime utc = pt::second_clock::universal_time();
	pt::ptime now;

	Poco::AutoPtr<Poco::Util::AbstractConfiguration> timeConf(config().createView("Time"));
	string timeType = timeConf->getString("Type","Local");

	if (boost::iequals(timeType,"Custom"))
		now = utc + pt::duration_from_string(timeConf->getString("Offset","0"));
	else if (boost::iequals(timeType,"Static"))
	{
		now = utc;
		now -= pt::time_duration(now.time_of_day().hours(),0,0);
		now += pt::time_duration(timeConf->getInt("Hour",8),0,0);
	}
	else
		now = pt::second_clock::local_time();

	_timeOffset =  now - utc;
}

#include "Version.h"
#include <Poco/Glob.h>

int HiveExtApp::main( const std::vector<std::string>& args )
{
	_logger.information("HiveExt " + GIT_VERSION.substr(0,12));
	setupClock();

	if (!this->initialiseService())
		return EXIT_IOERR;

	return EXIT_OK;
}

HiveExtApp::HiveExtApp(string suffixDir) : AppServer("HiveExt",suffixDir), _serverId(-1), _logger(Poco::Logger::get(""))
{
	//server and object stuff
	handlers[302] = boost::bind(&HiveExtApp::streamObjects,this,_1);
	handlers[303] = boost::bind(&HiveExtApp::objectInventory,this,_1,false);
	handlers[304] = boost::bind(&HiveExtApp::objectDelete,this,_1,false);
	handlers[305] = boost::bind(&HiveExtApp::vehicleMoved,this,_1);
	handlers[306] = boost::bind(&HiveExtApp::vehicleDamaged,this,_1);
	handlers[307] = boost::bind(&HiveExtApp::getDateTime,this,_1);
	handlers[308] = boost::bind(&HiveExtApp::objectPublish,this,_1);
	handlers[309] = boost::bind(&HiveExtApp::objectInventory,this,_1,true);
	handlers[310] = boost::bind(&HiveExtApp::objectDelete,this,_1,true);
	//player/character loads
	handlers[101] = boost::bind(&HiveExtApp::loadPlayer,this,_1);
	handlers[102] = boost::bind(&HiveExtApp::loadCharacterDetails,this,_1);
	handlers[103] = boost::bind(&HiveExtApp::recordCharacterLogin,this,_1);
	//character updates
	handlers[201] = boost::bind(&HiveExtApp::playerUpdate,this,_1);
	handlers[202] = boost::bind(&HiveExtApp::playerDeath,this,_1);
	handlers[203] = boost::bind(&HiveExtApp::playerInit,this,_1);
}

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;
using boost::bad_lexical_cast;

void HiveExtApp::callExtension( const char* function, char* output, size_t outputSize )
{
	Sqf::Parameters params;
	try
	{
		params = lexical_cast<Sqf::Parameters>(function);	
	}
	catch(bad_lexical_cast)
	{
		_logger.error("Cannot parse function: " + string(function));
		return;
	}

	int funcNum = -1;
	try
	{
		string childIdent = boost::get<string>(params.at(0));
		if (childIdent != "CHILD")
			throw std::runtime_error("First element in parameters must be CHILD");

		params.erase(params.begin());
		funcNum = boost::get<int>(params.at(0));
		params.erase(params.begin());
	}
	catch (...)
	{
		_logger.error("Invalid function format: " + string(function));
		return;
	}

	if (handlers.count(funcNum) < 1)
	{
		_logger.error("Invalid method id: " + lexical_cast<string>(funcNum));
		return;
	}

	if (_logger.debug())
		_logger.debug("Original params: |" + string(function) + "|");

	_logger.information("Method: " + lexical_cast<string>(funcNum) + " Params: " + lexical_cast<string>(params));
	HandlerFunc handler = handlers[funcNum];
	Sqf::Value res;
	try
	{
		res = handler(params);
	}
	catch (...)
	{
		_logger.error("Error executing |" + string(function) + "|");
		return;
	}		

	string serializedRes = lexical_cast<string>(res);
	if (serializedRes.length() >= outputSize)
	{
		_logger.error("Output size too big ("+lexical_cast<string>(serializedRes.length())+") for request : " + string(function));
		return;
	}

	_logger.information("Result: " + serializedRes);		
	strncpy_s(output,outputSize,serializedRes.c_str(),outputSize-1);
}

Sqf::Parameters HiveExtApp::booleanReturn( bool isGood )
{
	Sqf::Parameters retVal;
	string retStatus = "PASS";
	if (!isGood)
		retStatus = "ERROR";

	retVal.push_back(retStatus);
	return retVal;
}

Sqf::Value HiveExtApp::getDateTime( Sqf::Parameters params )
{
	namespace pt=boost::posix_time;
	pt::ptime now = pt::second_clock::universal_time() + _timeOffset;

	Sqf::Parameters retVal;
	retVal.push_back(string("PASS"));
	{
		Sqf::Parameters dateTime;
		dateTime.push_back(static_cast<int>(now.date().year()));
		dateTime.push_back(static_cast<int>(now.date().month()));
		dateTime.push_back(static_cast<int>(now.date().day()));
		dateTime.push_back(static_cast<int>(now.time_of_day().hours()));
		dateTime.push_back(static_cast<int>(now.time_of_day().minutes()));
		retVal.push_back(dateTime);
	}
	return retVal;
}

#include "DataSource/ObjDataSource.h"

Sqf::Value HiveExtApp::streamObjects( Sqf::Parameters params )
{
	if (_srvObjects.empty())
	{
		int serverId = boost::get<int>(params.at(0));
		setServerId(serverId);

		_objData->populateObjects(getServerId(), _srvObjects);

		Sqf::Parameters retVal;
		retVal.push_back(string("ObjectStreamStart"));
		retVal.push_back(static_cast<int>(_srvObjects.size()));
		return retVal;
	}
	else
	{
		Sqf::Parameters retVal = _srvObjects.front();
		_srvObjects.pop();

		return retVal;
	}
}

Sqf::Value HiveExtApp::objectInventory( Sqf::Parameters params, bool byUID /*= false*/ )
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));
	Sqf::Value inventory = boost::get<Sqf::Parameters>(params.at(1));

	if (objectIdent != 0) //all the vehicles have objectUID = 0, so it would be bad to update those
		return booleanReturn(_objData->updateObjectInventory(getServerId(),objectIdent,byUID,inventory));

	return booleanReturn(true);
}

Sqf::Value HiveExtApp::objectDelete( Sqf::Parameters params, bool byUID /*= false*/ )
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));

	if (objectIdent != 0) //all the vehicles have objectUID = 0, so it would be bad to delete those
		return booleanReturn(_objData->deleteObject(getServerId(),objectIdent,byUID));

	return booleanReturn(true);
}

Sqf::Value HiveExtApp::vehicleMoved( Sqf::Parameters params )
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));
	Sqf::Value worldspace = boost::get<Sqf::Parameters>(params.at(1));
	double fuel = Sqf::GetDouble(params.at(2));

	if (objectIdent > 0) //sometimes script sends this with object id 0, which is bad
		return booleanReturn(_objData->updateVehicleMovement(getServerId(),objectIdent,worldspace,fuel));

	return booleanReturn(true);
}

Sqf::Value HiveExtApp::vehicleDamaged( Sqf::Parameters params )
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));
	Sqf::Value hitPoints = boost::get<Sqf::Parameters>(params.at(1));
	double damage = Sqf::GetDouble(params.at(2));

	if (objectIdent > 0) //sometimes script sends this with object id 0, which is bad
		return booleanReturn(_objData->updateVehicleStatus(getServerId(),objectIdent,hitPoints,damage));

	return booleanReturn(true);
}

Sqf::Value HiveExtApp::objectPublish( Sqf::Parameters params )
{
	string className = boost::get<string>(params.at(1));
	double damage = Sqf::GetDouble(params.at(2));
	int characterId = Sqf::GetIntAny(params.at(3));
	Sqf::Value worldSpace = boost::get<Sqf::Parameters>(params.at(4));
	Sqf::Value inventory = boost::get<Sqf::Parameters>(params.at(5));
	Sqf::Value hitPoints = boost::get<Sqf::Parameters>(params.at(6));
	double fuel = Sqf::GetDouble(params.at(7));
	Int64 uniqueId = Sqf::GetBigInt(params.at(8));

	return booleanReturn(_objData->createObject(getServerId(),className,damage,characterId,worldSpace,inventory,hitPoints,fuel,uniqueId));
}

#include "DataSource/CharDataSource.h"

Sqf::Value HiveExtApp::loadPlayer( Sqf::Parameters params )
{
	string playerId = Sqf::GetStringAny(params.at(0));
	string playerName = Sqf::GetStringAny(params.at(2));

	return _charData->fetchCharacterInitial(playerId,getServerId(),playerName);
}

Sqf::Value HiveExtApp::loadCharacterDetails( Sqf::Parameters params )
{
	int characterId = Sqf::GetIntAny(params.at(0));
	
	return _charData->fetchCharacterDetails(characterId);
}

Sqf::Value HiveExtApp::recordCharacterLogin( Sqf::Parameters params )
{
	string playerId = Sqf::GetStringAny(params.at(0));
	int characterId = Sqf::GetIntAny(params.at(1));
	int action = Sqf::GetIntAny(params.at(2));

	return booleanReturn(_charData->recordLogin(playerId,characterId,action));
}

Sqf::Value HiveExtApp::playerUpdate( Sqf::Parameters params )
{
	int characterId = Sqf::GetIntAny(params.at(0));
	CharDataSource::FieldsType fields;

	try
	{
		if (!Sqf::IsNull(params.at(1)))
		{
			Sqf::Parameters worldSpaceArr = boost::get<Sqf::Parameters>(params.at(1));
			if (worldSpaceArr.size() > 0)
			{
				Sqf::Value worldSpace = worldSpaceArr;
				fields["Worldspace"] = worldSpace;
			}
		}
		if (!Sqf::IsNull(params.at(2)))
		{
			Sqf::Parameters inventoryArr = boost::get<Sqf::Parameters>(params.at(2));
			if (inventoryArr.size() > 0)
			{
				Sqf::Value inventory = inventoryArr;
				fields["Inventory"] = inventory;
			}
		}
		if (!Sqf::IsNull(params.at(3)))
		{
			Sqf::Parameters backpackArr = boost::get<Sqf::Parameters>(params.at(3));
			if (backpackArr.size() > 0)
			{
				Sqf::Value backpack = backpackArr;
				fields["Backpack"] = backpack;
			}
		}
		if (!Sqf::IsNull(params.at(4)))
		{
			Sqf::Parameters medicalArr = boost::get<Sqf::Parameters>(params.at(4));
			if (medicalArr.size() > 0)
			{
				for (size_t i=0;i<medicalArr.size();i++)
				{
					if (Sqf::IsAny(medicalArr[i]))
					{
						_logger.warning("update.medical["+lexical_cast<string>(i)+"] changed from any to []");
						medicalArr[i] = Sqf::Parameters();
					}
				}
				Sqf::Value medical = medicalArr;
				fields["Medical"] = medical;
			}
		}
		if (!Sqf::IsNull(params.at(5)))
		{
			bool justAte = boost::get<bool>(params.at(5));
			if (justAte) fields["JustAte"] = true;
		}
		if (!Sqf::IsNull(params.at(6)))
		{
			bool justDrank = boost::get<bool>(params.at(6));
			if (justDrank) fields["JustDrank"] = true;
		}
		if (!Sqf::IsNull(params.at(7)))
		{
			int moreKillsZ = boost::get<int>(params.at(7));
			if (moreKillsZ > 0) fields["KillsZ"] = moreKillsZ;
		}
		if (!Sqf::IsNull(params.at(8)))
		{
			int moreKillsH = boost::get<int>(params.at(8));
			if (moreKillsH > 0) fields["HeadshotsZ"] = moreKillsH;
		}
		if (!Sqf::IsNull(params.at(9)))
		{
			int distanceWalked = static_cast<int>(Sqf::GetDouble(params.at(9)));
			if (distanceWalked > 0) fields["DistanceFoot"] = distanceWalked;
		}
		if (!Sqf::IsNull(params.at(10)))
		{
			int durationLived = static_cast<int>(Sqf::GetDouble(params.at(10)));
			if (durationLived > 0) fields["Duration"] = durationLived;
		}
		if (!Sqf::IsNull(params.at(11)))
		{
			Sqf::Parameters currentStateArr = boost::get<Sqf::Parameters>(params.at(11));
			if (currentStateArr.size() > 0)
			{
				Sqf::Value currentState = currentStateArr;
				fields["CurrentState"] = currentState;
			}
		}
		if (!Sqf::IsNull(params.at(12)))
		{
			int moreKillsHuman = boost::get<int>(params.at(12));
			if (moreKillsHuman > 0) fields["KillsH"] = moreKillsHuman;
		}
		if (!Sqf::IsNull(params.at(13)))
		{
			int moreKillsBandit = boost::get<int>(params.at(13));
			if (moreKillsBandit > 0) fields["KillsB"] = moreKillsBandit;
		}
		if (!Sqf::IsNull(params.at(14)))
		{
			string newModel = boost::get<string>(params.at(14));
			fields["Model"] = newModel;
		}
		if (!Sqf::IsNull(params.at(15)))
		{
			int humanityDiff = static_cast<int>(Sqf::GetDouble(params.at(15)));
			if (humanityDiff != 0) fields["Humanity"] = humanityDiff;
		}
	}
	catch (const std::out_of_range&)
	{
		_logger.warning("Update of character " + lexical_cast<string>(characterId) + " only had " + lexical_cast<string>(params.size()) + " parameters out of 16");
	}

	if (fields.size() > 0)
		return booleanReturn(_charData->updateCharacter(characterId,fields));

	return booleanReturn(true);
}

Sqf::Value HiveExtApp::playerInit( Sqf::Parameters params )
{
	int characterId = Sqf::GetIntAny(params.at(0));
	Sqf::Value inventory = boost::get<Sqf::Parameters>(params.at(1));
	Sqf::Value backpack = boost::get<Sqf::Parameters>(params.at(2));

	return booleanReturn(_charData->initCharacter(characterId,inventory,backpack));
}

Sqf::Value HiveExtApp::playerDeath( Sqf::Parameters params )
{
	int characterId = Sqf::GetIntAny(params.at(0));
	int duration = static_cast<int>(Sqf::GetDouble(params.at(1)));
	
	return booleanReturn(_charData->killCharacter(characterId,duration));
}