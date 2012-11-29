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

#include "DatabaseLoader.h"
#include <Poco/SingletonHolder.h>

#include "Shared/Library/SharedLibraryLoader.h"
#include <Poco/Util/AbstractConfiguration.h>
#include <Poco/String.h>
#include <Poco/NumberFormatter.h>
#include <Poco/Format.h>

namespace
{
	extern const char libName[] = "Database" ; 
	typedef SharedLibraryLoader<Database, libName> LibraryType;
	static Poco::SingletonHolder<LibraryType> holder;
}

shared_ptr<Database> DatabaseLoader::create(DBType dbType)
{
	std::string dbTypeStr;
	switch (dbType)
	{
	case DBTYPE_MYSQL:
		dbTypeStr = "DatabaseMysql";
		break;
	}

	if (!holder.get()->canCreate(dbTypeStr))
		throw CreationError(string("Unimplemented database type: ") + dbTypeStr);

	return shared_ptr<Database>(holder.get()->create(dbTypeStr));
}

namespace
{
	bool GetDBTypeFromConfig(Poco::Util::AbstractConfiguration* dbConfig, DatabaseLoader::DBType& outType)
	{
		string dbTypeStr;
		if (dbConfig->has("Type"))
			dbTypeStr = dbConfig->getString("Type");
		else if (dbConfig->has("Provider"))
			dbTypeStr = dbConfig->getString("Provider");
		else if (dbConfig->has("Engine"))
			dbTypeStr = dbConfig->getString("Engine");
		else
			dbTypeStr = "MySQL";

		Poco::toLowerInPlace(dbTypeStr);

		DatabaseLoader::DBType dbTypeNum;
		if (dbTypeStr.find("mysql") != string::npos)
			dbTypeNum = DatabaseLoader::DBTYPE_MYSQL;
		else
			return false;

		outType = dbTypeNum;
		return true;
	}
};

shared_ptr<Database> DatabaseLoader::create( Poco::Util::AbstractConfiguration* dbConfig )
{
	DBType dbTypeNum;
	if (!GetDBTypeFromConfig(dbConfig,dbTypeNum))
		throw CreationError(string("Unrecognised DB type"));

	return create(dbTypeNum);
}

string DatabaseLoader::makeInitString(Poco::Util::AbstractConfiguration* dbConfig, const string& defUser, const string& defPass, const string& defDbName, const string& defDbHost)
{
	string host = dbConfig->getString("Host",defDbHost);

	DBType dbTypeNum;
	string socket_or_port;
	if (dbConfig->has("Port"))
		socket_or_port = Poco::NumberFormatter::format(dbConfig->getInt("Port"));
	else if (dbConfig->has("Socket"))
		socket_or_port = dbConfig->getString("Socket");
	else if (GetDBTypeFromConfig(dbConfig,dbTypeNum))
	{
		if (dbTypeNum == DBTYPE_MYSQL)
			socket_or_port = "3306";
	}

	string username = dbConfig->getString("Username",defUser);
	string password = dbConfig->getString("Password",defPass);
	string database = dbConfig->getString("Database",defDbName);

	return DatabaseLoader::makeInitString(host,socket_or_port,username,password,database);
}

string DatabaseLoader::makeInitString(const string& host, const string& socket_or_port, const string& username, const string& password, const string& database)
{
	return Poco::format("%s;%s;%s;%s;%s",host,socket_or_port,username,password,database);
}


