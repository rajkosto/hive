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

string DatabaseLoader::GetDbTypeFromConfig( Poco::Util::AbstractConfiguration* dbConfig )
{
	string dbTypeStr;

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

	return dbTypeStr;
}

#include <boost/algorithm/string/predicate.hpp>

string DatabaseLoader::GetDbModuleName( string dbType, bool physicalName )
{
	if (boost::icontains(dbType,"mysql"))
		dbType = "MySql";
	else if (boost::icontains(dbType,"postgre"))
		dbType = "Postgre";

	string modName = "Database"+dbType;
	if (physicalName)
		modName += Poco::SharedLibrary::suffix();

	return modName;
}

bool DatabaseLoader::GetVersionOfModule( const string& moduleName, UInt32& outMajor, UInt32& outMinor, UInt32& outRev, UInt32& outBld )
{
	std::wstring fileName;
	{
		WCHAR fullPath[MAX_PATH];
		HMODULE ourModule = GetModuleHandleA(moduleName.c_str());
		if (ourModule == NULL)
			return false;

		GetModuleFileNameW(ourModule,fullPath,MAX_PATH);
		fileName = fullPath;
	}

	size_t verSize = GetFileVersionInfoSizeW(fileName.c_str(),NULL);
	if (verSize < 1)
		return false;

	vector<UInt8> fileVerBuf(verSize);
	if (!GetFileVersionInfoW(fileName.c_str(),0,fileVerBuf.size(),&fileVerBuf[0]))
		return false;

	VS_FIXEDFILEINFO* fileInfo = NULL;
	size_t infoLen = 0;
	if (!VerQueryValue(&fileVerBuf[0],L"\\",(LPVOID*)&fileInfo,&infoLen))
		return false;
	if (!fileInfo || infoLen < 1)
		return false;

	outMajor	= HIWORD(fileInfo->dwFileVersionMS);
	outMinor	= LOWORD(fileInfo->dwFileVersionMS);
	outRev		= HIWORD(fileInfo->dwFileVersionLS);
	outBld		= LOWORD(fileInfo->dwFileVersionLS);

	return true;
}

bool DatabaseLoader::IsVersionCompatible( const UInt32* wantedVer, const UInt32* gotVer )
{
	if (gotVer[0] != wantedVer[0])
		return false;
	if (gotVer[1] != wantedVer[1])
		return false;
	if (gotVer[2] != wantedVer[2])
		return false;
	if (gotVer[3] < wantedVer[3])
		return false;

	return true;
}

shared_ptr<Database> DatabaseLoader::Create(const string& dbType)
{
	const string moduleName = GetDbModuleName(dbType);
	try
	{
		return shared_ptr<Database>(holder.get()->create(moduleName));
	}
	catch (const Poco::NotFoundException&)
	{
		try
		{
			holder.get()->loadLibrary(moduleName);
			
			UInt32 dbVerNum[4];	
			const string fullLibName = GetDbModuleName(dbType,true);					
			if (!GetVersionOfModule(fullLibName,dbVerNum[0],dbVerNum[1],dbVerNum[2],dbVerNum[3]))
				throw CreationError("Unable to get "+moduleName+" module version info");

			string wantedDbVerStr = Poco::format("%u.%u.%u.%u or higher, but lower than ",
				REQUIRED_DB_VERSION_NUM[0],REQUIRED_DB_VERSION_NUM[1],REQUIRED_DB_VERSION_NUM[2],REQUIRED_DB_VERSION_NUM[3]);
			wantedDbVerStr += Poco::format("%u.%u.%u.0",REQUIRED_DB_VERSION_NUM[0],REQUIRED_DB_VERSION_NUM[1],REQUIRED_DB_VERSION_NUM[2]+1);

			if (!IsVersionCompatible(REQUIRED_DB_VERSION_NUM,dbVerNum))
			{
				throw CreationError(Poco::format(moduleName+" module is incompatible (%u.%u.%u.%u). Replace it with a compatible version (%s)",
					dbVerNum[0],dbVerNum[1],dbVerNum[2],dbVerNum[3],wantedDbVerStr));
			}

			return shared_ptr<Database>(holder.get()->create(moduleName));
		}
		catch (const Poco::LibraryLoadException&) { throw CreationError("Error loading database module: "+moduleName); }
		catch (const Poco::NotFoundException&) { throw CreationError("Unimplemented database type: "+dbType); }
	}
}

shared_ptr<Database> DatabaseLoader::Create( Poco::Util::AbstractConfiguration* dbConfig )
{
	return Create(GetDbTypeFromConfig(dbConfig));
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


