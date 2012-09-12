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

#include "DirectHiveApp.h"

DirectHiveApp::DirectHiveApp(string suffixDir) : HiveExtApp(suffixDir) {}

#include "Shared/Library/Database/DatabaseLoader.h"
#include "HiveLib/DataSource/SqlCharDataSource.h"
#include "HiveLib/DataSource/SqlObjDataSource.h"

bool DirectHiveApp::initialiseService()
{
	_charDb = DatabaseLoader::create(DatabaseLoader::DBTYPE_MYSQL);

	Poco::Logger& dbLogger = Poco::Logger::get("Database");
	dbLogger.setLevel("trace");

	string initString;
	{
		Poco::AutoPtr<Poco::Util::AbstractConfiguration> globalDBConf(config().createView("Database"));
		initString = DatabaseLoader::makeInitString(globalDBConf);
	}

	if (!_charDb->Initialize(dbLogger,initString))
		return false;

	_charDb->AllowAsyncTransactions();
	_objDb = _charDb;
	
	//pass the db along to character datasource
	{
		Poco::AutoPtr<Poco::Util::AbstractConfiguration> charDBConf(config().createView("Characters"));
		_charData.reset(new SqlCharDataSource(_logger,
			_charDb,charDBConf->getString("IDField","PlayerUID"),charDBConf->getString("WSField","Worldspace")));	
	}

	Poco::AutoPtr<Poco::Util::AbstractConfiguration> objDBConf(config().createView("ObjectDB"));
	bool useExternalObjDb = objDBConf->getBool("Use",false);
	if (useExternalObjDb)
	{
		try { _objDb = DatabaseLoader::create(objDBConf); } 
		catch (const DatabaseLoader::CreationError&) { return false; }
		
		Poco::Logger& objDBLogger = Poco::Logger::get("ObjectDB");
		objDBLogger.setLevel("trace");

		if (!_objDb->Initialize(objDBLogger,DatabaseLoader::makeInitString(objDBConf)))
			return false;

		_objDb->AllowAsyncTransactions();
	}
	
	Poco::AutoPtr<Poco::Util::AbstractConfiguration> objConf(config().createView("Objects"));
	_objData.reset(new SqlObjDataSource(_logger,_objDb,objConf.get()));
	
	return true;
}
