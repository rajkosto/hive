/*
* Copyright (C) 2009-2013 Rajko Stojadinovic <http://github.com/rajkosto/hive>
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

#include "Shared/Common/Types.h"
#include "Database/Database.h"
#include <Poco/Util/AbstractConfiguration.h>

static const UInt32 REQUIRED_DB_VERSION_NUM[4] = {0,9,14,0};

class DatabaseLoader
{
public:
	static string GetDbTypeFromConfig(Poco::Util::AbstractConfiguration* dbConfig);
	static string GetDbModuleName(string dbType, bool physicalName = false);

	static bool GetVersionOfModule(const string& moduleName, UInt32& outMajor, UInt32& outMinor, UInt32& outRev, UInt32& outBld);
	static bool IsVersionCompatible(const UInt32* wantedVer, const UInt32* gotVer);
	
	static shared_ptr<Database> Create(const string& dbType);
	static shared_ptr<Database> Create(Poco::Util::AbstractConfiguration* dbConfig);

	static Database::KeyValueColl MakeConnParams(Poco::Util::AbstractConfiguration* dbConfig);

	POCO_DEFINE_EXCEPTION(,CreationError,Poco::LogicException,"Cannot create database");
};