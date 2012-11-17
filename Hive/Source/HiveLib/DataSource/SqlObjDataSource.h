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

#pragma once

#include "SqlDataSource.h"
#include "ObjDataSource.h"
#include "Database/SqlStatement.h"

namespace Poco { namespace Util { class AbstractConfiguration; }; };
class SqlObjDataSource : public SqlDataSource, public ObjDataSource
{
public:
	SqlObjDataSource(Poco::Logger& logger, shared_ptr<Database> db, const Poco::Util::AbstractConfiguration* conf);
	~SqlObjDataSource() {}

	void populateObjects( int serverId, ServerObjectsQueue& queue ) override;
	bool updateObjectInventory( int serverId, Int64 objectIdent, bool byUID, const Sqf::Value& inventory ) override;
	bool deleteObject( int serverId, Int64 objectIdent, bool byUID ) override;
	bool updateVehicleMovement( int serverId, Int64 objectIdent, const Sqf::Value& worldspace, double fuel ) override;
	bool updateVehicleStatus( int serverId, Int64 objectIdent, const Sqf::Value& hitPoints, double damage ) override;
	bool createObject( int serverId, const string& className, double damage, int characterId, 
		const Sqf::Value& worldSpace, const Sqf::Value& inventory, const Sqf::Value& hitPoints, double fuel, Int64 uniqueId ) override;
private:
	string _objTableName;
	int _cleanupPlacedDays;
	bool _vehicleOOBReset;

	//statement ids
	SqlStatementID _stmtDeleteOldObject;
	SqlStatementID _stmtUpdateObjectbyUID;
	SqlStatementID _stmtUpdateObjectByID;
	SqlStatementID _stmtDeleteObjectByUID;
	SqlStatementID _stmtDeleteObjectByID;
	SqlStatementID _stmtUpdateVehicleMovement;
	SqlStatementID _stmtUpdateVehicleStatus;
	SqlStatementID _stmtCreateObject;
};