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

#include <tbb/concurrent_queue.h>
#include <Poco/Runnable.h>

class Database;
class SqlOperation;
class SqlConnection;

class SqlDelayThread : public Poco::Runnable
{
protected:
	typedef tbb::concurrent_queue<SqlOperation*> SqlQueue;

	SqlQueue _sqlQueue;			//Queue of SQL statements
	Database& _dbEngine;		//Pointer to used Database engine
	SqlConnection& _dbConn;		//Pointer to DB connection
	volatile bool _isRunning;

	//process all enqueued requests
	virtual void processRequests();
public:
	SqlDelayThread(Database& db, SqlConnection& conn);
	virtual ~SqlDelayThread();

	//Put sql statement to delay queue
	bool queueOperation(SqlOperation* sql) 
	{
		_sqlQueue.push(sql);
		return true; 
	}

	//Send stop event
	virtual void stop();
	//Main Thread loop
	void run() override;
};