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

SqlDelayThread::SqlDelayThread(Database* db, SqlConnection* conn) : m_dbEngine(db), m_dbConnection(conn), m_running(true)
{
}

SqlDelayThread::~SqlDelayThread()
{
    //process all requests which might have been queued while thread was stopping
    ProcessRequests();
}

void SqlDelayThread::run()
{
    const UInt32 loopSleepms = 10;

    const UInt32 pingEveryLoop = m_dbEngine->GetPingInterval() / loopSleepms;

    UInt32 loopCounter = 0;
    while (m_running)
    {
        // if the running state gets turned off while sleeping
        // empty the queue before exiting
        Poco::Thread::sleep(loopSleepms);

        ProcessRequests();

        if((loopCounter++) >= pingEveryLoop)
        {
            loopCounter = 0;
            m_dbEngine->Ping();
        }
    }
}

void SqlDelayThread::Stop()
{
    m_running = false;
}

void SqlDelayThread::ProcessRequests()
{
    SqlOperation* s = NULL;
    while (m_sqlQueue.try_pop(s))
    {
        s->Execute(m_dbConnection);
        delete s;
    }
}
