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

namespace
{
	typedef SharedLibraryLoader<Database> LibraryType;
	static Poco::SingletonHolder<LibraryType> holder;
}

shared_ptr<Database> DatabaseLoader::Create(const string& dbType)
{
	try
	{
		return shared_ptr<Database>(holder.get()->create("Database"+dbType));
	}
	catch (const Poco::NotFoundException&)
	{
		throw CreationError("Unimplemented database type: " + dbType);
	}
}

shared_ptr<Database> DatabaseLoader::Create( Poco::Util::AbstractConfiguration* dbConfig )
{
	string dbTypeStr;
	{
		if (dbConfig->has("Type"))
			dbTypeStr = dbConfig->getString("Type");
		else if (dbConfig->has("Provider"))
			dbTypeStr = dbConfig->getString("Provider");
		else if (dbConfig->has("Engine"))
			dbTypeStr = dbConfig->getString("Engine");
		else
			dbTypeStr = "MySql";

		Poco::trimInPlace(dbTypeStr);

		if (dbTypeStr.length() < 1)
			throw DatabaseLoader::CreationError(string("Unspecified DB type"));
	}
	return Create(dbTypeStr);
}

Database::KeyValueColl DatabaseLoader::MakeConnParams(Poco::Util::AbstractConfiguration* dbConfig)
{
	Database::KeyValueColl keyVals;
	{
		vector<string> keys;
		dbConfig->keys(keys);

		for (auto it=keys.begin(); it!=keys.end(); ++it)
		{
			string value = dbConfig->getString(*it);
			Poco::trimInPlace(value);
			string keyStr = std::move(*it);
			Poco::toLowerInPlace(keyStr);
			keyVals.insert(std::make_pair(std::move(keyStr),std::move(value)));
		}
	}	

	return keyVals;
}


