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

#include "SqlObjDataSource.h"
#include "Database/Database.h"

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
using boost::lexical_cast;
using boost::bad_lexical_cast;

namespace
{
	typedef boost::optional<Sqf::Value> PositionInfo;
	class PositionFixerVisitor : public boost::static_visitor<PositionInfo>
	{
	public:
		PositionInfo operator()(Sqf::Parameters& pos) const 
		{ 
			if (pos.size() != 3)
				return PositionInfo();

			try
			{
				double x = Sqf::GetDouble(pos[0]);
				double y = Sqf::GetDouble(pos[1]);
				double z = Sqf::GetDouble(pos[2]);

				if (x < 0 || y > 15360)
				{
					PositionInfo fixed(pos);
					pos.clear();
					return fixed;
				}
			}
			catch(const boost::bad_get&) {}

			return PositionInfo();
		}
		template<typename T> PositionInfo operator()(const T& other) const	{ return PositionInfo(); }
	};

	class WorldspaceFixerVisitor : public boost::static_visitor<PositionInfo>
	{
	public:
		PositionInfo operator()(Sqf::Parameters& ws) const 
		{ 
			if (ws.size() != 2)
				return PositionInfo();

			return boost::apply_visitor(PositionFixerVisitor(),ws[1]);
		}
		template<typename T> PositionInfo operator()(const T& other) const	{ return PositionInfo(); }
	};

	PositionInfo FixOOBWorldspace(Sqf::Value& v) { return boost::apply_visitor(WorldspaceFixerVisitor(),v); }
};

#include <Poco/Util/AbstractConfiguration.h>
SqlObjDataSource::SqlObjDataSource( Poco::Logger& logger, shared_ptr<Database> db, const Poco::Util::AbstractConfiguration* conf ) : SqlDataSource(logger,db)
{
	static const string defaultTable = "Object_DATA"; 
	if (conf != NULL)
	{
		_objTableName = getDB()->escape_string(conf->getString("Table",defaultTable));
		_cleanupPlacedDays = conf->getInt("CleanupPlacedAfterDays",6);
		_vehicleOOBReset = conf->getBool("ResetOOBVehicles",false);
	}
	else
	{
		_objTableName = defaultTable;
		_cleanupPlacedDays = -1;
		_vehicleOOBReset = false;
	}
}

void SqlObjDataSource::populateObjects( int serverId, ServerObjectsQueue& queue )
{
	if (_cleanupPlacedDays >= 0)
	{
		string commonSql = "FROM `"+_objTableName+"` WHERE `Instance` = " + lexical_cast<string>(serverId) +
			" AND `ObjectUID` <> 0 AND `CharacterID` <> 0" +
			" AND `Datestamp` < DATE_SUB(CURRENT_TIMESTAMP, INTERVAL "+lexical_cast<string>(_cleanupPlacedDays)+" DAY)" +
			" AND ( (`Inventory` IS NULL) OR (`Inventory` = '[]') )";

		int numCleaned = 0;
		{
			scoped_ptr<QueryResult> numObjsToClean(getDB()->Query(("SELECT COUNT(*) "+commonSql).c_str()));
			if (numObjsToClean)
			{
				Field* fields = numObjsToClean->Fetch();
				numCleaned = fields[0].GetInt32();
			}
		}
		if (numCleaned > 0)
		{
			_logger.information("Removing " + lexical_cast<string>(numCleaned) + " empty placed objects older than " + lexical_cast<string>(_cleanupPlacedDays) + " days");

			scoped_ptr<SqlStatement> stmt(getDB()->CreateStatement(_stmtDeleteOldObject, "DELETE "+commonSql));
			if (!stmt->DirectExecute())
				_logger.error("Error executing placed objects cleanup statement");
		}
	}
	
	scoped_ptr<QueryResult> worldObjsRes(getDB()->PQuery("SELECT `ObjectID`, `Classname`, `CharacterID`, `Worldspace`, `Inventory`, `Hitpoints`, `Fuel`, `Damage` FROM `%s` WHERE `Instance`=%d AND `Classname` IS NOT NULL", _objTableName.c_str(), serverId));
	if (worldObjsRes) do
	{
		Field* fields = worldObjsRes->Fetch();
		Sqf::Parameters objParams;
		objParams.push_back(string("OBJ"));

		int objectId = fields[0].GetInt32();
		objParams.push_back(lexical_cast<string>(objectId)); //objectId should be stringified
		try
		{
			objParams.push_back(fields[1].GetCppString()); //classname
			objParams.push_back(lexical_cast<string>(fields[2].GetInt32())); //ownerId should be stringified

			Sqf::Value worldSpace = lexical_cast<Sqf::Value>(fields[3].GetString());
			if (_vehicleOOBReset && fields[2].GetInt32() == 0) // no owner = vehicle
			{
				PositionInfo posInfo = FixOOBWorldspace(worldSpace);
				if (posInfo.is_initialized())
					_logger.information("Reset ObjectID " + lexical_cast<string>(objectId) + " (" + fields[1].GetCppString() + ") from position " + lexical_cast<string>(*posInfo));

			}
			
			objParams.push_back(worldSpace);

			//Inventory can be NULL
			{
				string invStr = "[]";
				if (!fields[4].IsNULL())
					invStr = fields[4].GetCppString();

				objParams.push_back(lexical_cast<Sqf::Value>(invStr));
			}	
			objParams.push_back(lexical_cast<Sqf::Value>(fields[5].GetString()));
			objParams.push_back(fields[6].GetDouble());
			objParams.push_back(fields[7].GetDouble());
		}
		catch (bad_lexical_cast)
		{
			_logger.error("Skipping ObjectID " + lexical_cast<string>(objectId) + " load because of invalid data in db");
			continue;
		}

		queue.push(objParams);
	} while(worldObjsRes->NextRow());
}

bool SqlObjDataSource::updateObjectInventory( int serverId, Int64 objectIdent, bool byUID, const Sqf::Value& inventory )
{
	scoped_ptr<SqlStatement> stmt;
	if (byUID)
		stmt.reset(getDB()->CreateStatement(_stmtUpdateObjectbyUID, "UPDATE `"+_objTableName+"` SET `Inventory` = ? WHERE `ObjectUID` = ?"));
	else
		stmt.reset(getDB()->CreateStatement(_stmtUpdateObjectByID, "UPDATE `"+_objTableName+"` SET `Inventory` = ? WHERE `ObjectID` = ?"));

	stmt->addString(lexical_cast<string>(inventory));
	stmt->addInt64(objectIdent);

	bool exRes = stmt->Execute();
	poco_assert(exRes == true);

	return exRes;
}

bool SqlObjDataSource::deleteObject( int serverId, Int64 objectIdent, bool byUID )
{
	scoped_ptr<SqlStatement> stmt;
	if (byUID)
		stmt.reset(getDB()->CreateStatement(_stmtDeleteObjectByUID, "DELETE FROM `"+_objTableName+"` WHERE `ObjectUID` = ?"));
	else
		stmt.reset(getDB()->CreateStatement(_stmtDeleteObjectByID, "DELETE FROM `"+_objTableName+"` WHERE `ObjectID` = ?"));

	stmt->addInt64(objectIdent);

	bool exRes = stmt->Execute();
	poco_assert(exRes == true);

	return exRes;
}

bool SqlObjDataSource::updateVehicleMovement( int serverId, Int64 objectIdent, const Sqf::Value& worldspace, double fuel )
{
	scoped_ptr<SqlStatement> stmt(getDB()->CreateStatement(_stmtUpdateVehicleMovement, "UPDATE `"+_objTableName+"` SET `Worldspace` = ? , `Fuel` = ? WHERE `ObjectID` = ?"));
	stmt->addString(lexical_cast<string>(worldspace));
	stmt->addDouble(fuel);
	stmt->addInt64(objectIdent);
	bool exRes = stmt->Execute();
	poco_assert(exRes == true);

	return exRes;
}

bool SqlObjDataSource::updateVehicleStatus( int serverId, Int64 objectIdent, const Sqf::Value& hitPoints, double damage )
{
	scoped_ptr<SqlStatement> stmt(getDB()->CreateStatement(_stmtUpdateVehicleStatus, "UPDATE `"+_objTableName+"` SET `Hitpoints` = ? , `Damage` = ? WHERE `ObjectID` = ?"));
	stmt->addString(lexical_cast<string>(hitPoints));
	stmt->addDouble(damage);
	stmt->addInt64(objectIdent);
	bool exRes = stmt->Execute();
	poco_assert(exRes == true);

	return exRes;
}

bool SqlObjDataSource::createObject( int serverId, const string& className, double damage, int characterId, 
	const Sqf::Value& worldSpace, const Sqf::Value& inventory, const Sqf::Value& hitPoints, double fuel, Int64 uniqueId )
{
	scoped_ptr<SqlStatement> stmt(getDB()->CreateStatement(_stmtCreateObject, 
		"INSERT INTO `"+_objTableName+"` (`ObjectUID`, `Instance`, `Classname`, `Damage`, `CharacterID`, `Worldspace`, `Inventory`, `Hitpoints`, `Fuel`, `Datestamp`) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)"));

	stmt->addInt64(uniqueId);
	stmt->addInt32(serverId);
	stmt->addString(className);
	stmt->addDouble(damage);
	stmt->addInt32(characterId);
	stmt->addString(lexical_cast<string>(worldSpace));
	stmt->addString(lexical_cast<string>(inventory));
	stmt->addString(lexical_cast<string>(hitPoints));
	stmt->addDouble(fuel);
	bool exRes = stmt->Execute();
	poco_assert(exRes == true);

	return exRes;
}

