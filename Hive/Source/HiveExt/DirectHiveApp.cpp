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
	//Load up databases
	{
		Poco::AutoPtr<Poco::Util::AbstractConfiguration> globalDBConf(config().createView("Database"));
		Poco::AutoPtr<Poco::Util::AbstractConfiguration> objDBConf(config().createView("ObjectDB"));

		try
		{
			Poco::Logger& dbLogger = Poco::Logger::get("Database");
			_charDb = DatabaseLoader::Create(globalDBConf);
			if (!_charDb->initialise(dbLogger,DatabaseLoader::MakeConnParams(globalDBConf)))
				return false;

			_objDb = _charDb;
			if (objDBConf->getBool("Use",false))
			{
				Poco::Logger& objDBLogger = Poco::Logger::get("ObjectDB");
				_objDb = DatabaseLoader::Create(objDBConf);
				if (!_objDb->initialise(objDBLogger,DatabaseLoader::MakeConnParams(objDBConf)))
					return false;
			}
		}
		catch (const DatabaseLoader::CreationError& e) 
		{
			logger().critical(e.displayText());
			return false;
		}
	}

	//Create character datasource
	{
		static const string defaultID = "PlayerUID";
		static const string defaultWS = "Worldspace";

		Poco::AutoPtr<Poco::Util::AbstractConfiguration> charDBConf(config().createView("Characters"));
		_charData.reset(new SqlCharDataSource(logger(),_charDb,charDBConf->getString("IDField",defaultID),charDBConf->getString("WSField",defaultWS)));	
	}

	//Create object datasource
	{
		Poco::AutoPtr<Poco::Util::AbstractConfiguration> objConf(config().createView("Objects"));
		_objData.reset(new SqlObjDataSource(logger(),_objDb,objConf.get()));
	}

	//Create custom datasource
	_customData.reset(new CustomDataSource(logger(),_charDb,_objDb));

	_charDb->allowAsyncOperations();	
	if (_objDb != _charDb)
		_objDb->allowAsyncOperations();
	
	return true;
}
