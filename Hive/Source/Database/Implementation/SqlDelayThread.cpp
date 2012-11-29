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

#include "SqlDelayThread.h"
#include "Database/Database.h"
#include "SqlOperations.h"

#include <Poco/Thread.h>

SqlDelayThread::SqlDelayThread(Database& db, SqlConnection& conn) : _dbEngine(db), _dbConn(conn), _isRunning(true)
{
}

SqlDelayThread::~SqlDelayThread()
{
	//make sure we are stopped
	stop();
    //process all requests which might have been queued while thread was stopping
    processRequests();
}

void SqlDelayThread::run()
{
	_dbEngine.threadEnter();

    const size_t loopSleepMS = 10;

    while (_isRunning)
    {
        //if the running state gets turned off while sleeping
        //empty the queue before exiting
        Poco::Thread::sleep(loopSleepMS);

        processRequests();
    }

	_dbEngine.threadExit();
}

void SqlDelayThread::stop()
{
    _isRunning = false;
}

void SqlDelayThread::processRequests()
{
    SqlOperation* s = nullptr;
    while (_sqlQueue.try_pop(s))
    {
        s->execute(_dbConn);
        s->onRemove();
    }
}
