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

#include "Shared/Common/Types.h"
#include "Database/Database.h"
#include <Poco/Util/AbstractConfiguration.h>

class DatabaseLoader
{
public:
	enum DBType
	{
		DBTYPE_MYSQL,
	};
	static shared_ptr<Database> create(DBType dbType);
	static shared_ptr<Database> create(Poco::Util::AbstractConfiguration* dbConfig);
	static string makeInitString(Poco::Util::AbstractConfiguration* dbConfig, 
		const string& defUser = "root", const string& defPass = "", const string& defDbName = "dayz", const string& defDbHost = "localhost");
	static string makeInitString(const string& host, const string& socket_or_port, const string& username, const string& password, const string& database);
	POCO_DEFINE_EXCEPTION(,CreationError,Poco::LogicException,"Cannot create database");
};