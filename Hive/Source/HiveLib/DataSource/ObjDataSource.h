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

#include "DataSource.h"

class ObjDataSource
{
public:
	virtual ~ObjDataSource() {}

	typedef std::queue<Sqf::Parameters> ServerObjectsQueue;
	virtual void populateObjects( int serverId, ServerObjectsQueue& queue ) = 0;

	virtual bool updateObjectInventory( int serverId, Int64 objectIdent, bool byUID, const Sqf::Value& inventory ) = 0;
	virtual bool deleteObject( int serverId, Int64 objectIdent, bool byUID ) = 0;
	virtual bool updateVehicleMovement( int serverId, Int64 objectIdent, const Sqf::Value& worldspace, double fuel ) = 0;
	virtual bool updateVehicleStatus( int serverId, Int64 objectIdent, const Sqf::Value& hitPoints, double damage ) = 0;
	virtual bool createObject( int serverId, const string& className, double damage, int characterId, 
		const Sqf::Value& worldSpace, const Sqf::Value& inventory, const Sqf::Value& hitPoints, double fuel, Int64 uniqueId ) = 0;
};