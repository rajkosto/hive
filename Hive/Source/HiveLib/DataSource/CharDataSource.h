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

class CharDataSource
{
public:
	virtual ~CharDataSource() {}

	virtual Sqf::Value fetchCharacterInitial( string playerId, int serverId, const string& playerName ) = 0;
	virtual Sqf::Value fetchCharacterDetails( int characterId ) = 0;
	typedef map<string,Sqf::Value> FieldsType;
	virtual bool updateCharacter( int characterId, const FieldsType& fields ) = 0;
	virtual bool initCharacter( int characterId, const Sqf::Value& inventory, const Sqf::Value& backpack ) = 0;
	virtual bool killCharacter( int characterId, int duration ) = 0;
	virtual bool recordLogin( string playerId, int characterId, int action ) = 0;
protected:
	static int SanitiseInv(Sqf::Parameters& origInv);
};