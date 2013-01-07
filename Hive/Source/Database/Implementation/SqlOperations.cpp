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

#include "SqlOperations.h"
#include "Database/QueryResult.h"
#include "SqlDelayThread.h"
#include "SqlConnection.h"
#include "SqlPreparedStatement.h"

#include <Poco/Logger.h>
#include <Poco/Format.h>

#include "ConcreteDatabase.h"
#include "RetrySqlOp.h"


// ---- ASYNC STATEMENTS / TRANSACTIONS ----
bool SqlOperation::execute( SqlConnection& sqlConn )
{
	SqlConnection::Lock guard(sqlConn);
	return rawExecute(sqlConn);
}

bool SqlPlainRequest::rawExecute(SqlConnection& sqlConn, bool throwExc)
{
	//just do it
	return Retry::SqlOp<bool>(sqlConn.getDB().getLogger(),[&](SqlConnection& c){ return c.execute(_sql.c_str()); }, throwExc)
		(sqlConn,"PlainRequest",[&](){ return _sql; });
}

bool SqlTransaction::rawExecute(SqlConnection& sqlConn, bool throwExc)
{
	if(_queue.empty())
		return true;

	for (;;)
	{
		try
		{
			//the only time this returns false is when transactions aren't supported, so bail out
			//all other errors will throw a SqlException
			if (!sqlConn.transactionStart())
				return false;

			vector<SuccessCallback> callUsWhenDone;
			for (auto it=_queue.begin(); it!=_queue.end(); ++it)
			{
				SuccessCallback callMeOnDone;
				it->transExecute(sqlConn,callMeOnDone);

				if (!callMeOnDone.empty())
					callUsWhenDone.push_back(std::move(callMeOnDone));
			}

			poco_assert(sqlConn.transactionCommit() == true);
		
			//whole transaction came through, which means all the callbacks have good data
			for (size_t i=0; i<callUsWhenDone.size(); i++)
				callUsWhenDone[i]();

			break;
		}
		catch(const SqlConnection::SqlException& e)
		{
			//need to roll it back if the session hasn't ended
			if (!e.isConnLost())
			{
				//try a rollback command, if we get disconnected here then that's ok.
				try { poco_assert(sqlConn.transactionRollback() == true); }
				catch (const SqlConnection::SqlException& e)
				{ poco_assert(e.isConnLost() == true); }
			}
			else //conn lost, need to reconnect
			{
				try { sqlConn.connect(); }
				catch (const SqlConnection::SqlException& connExc)
				{
					//fatal error, cannot reach database anymore
					connExc.toLog(sqlConn.getDB().getLogger());
					if (throwExc)
						throw connExc;								
					else
						return false;
				}
			}

			if (throwExc)
				throw e;
			else if (e.isRepeatable())
				continue;
			else
				return false;
		}
	}

	return true;
}

bool SqlPreparedRequest::rawExecute(SqlConnection& sqlConn, bool throwExc)
{
	return Retry::SqlOp<bool>(sqlConn.getDB().getLogger(),[&](SqlConnection& c){ return c.executeStmt(_id, _params); }, throwExc)
		(sqlConn,"PreparedRequest",[&](){ return sqlConn.getStmt(_id)->getSqlString(true); });
}

// ---- ASYNC QUERIES ----
bool SqlQuery::rawExecute(SqlConnection& sqlConn, bool throwExc)
{
	if(!_queue)
		return false;

	//execute the query and store the result in the callback
	{
		auto res = Retry::SqlOp<unique_ptr<QueryResult>>(sqlConn.getDB().getLogger(),[&](SqlConnection& c){ return c.query(_sql.c_str()); }, throwExc)
			(sqlConn,"AsyncQuery",[&](){ return _sql; });
		_callback.setResult(res.release());
	}

	//this is set only if we're part of a transaction
	//we can only process the calback immediately if we aren't
	if (!throwExc)
	{
		//add the callback to the sql result queue of the thread it originated from
		_queue->push(_callback);
	}

	return true;
}

void SqlQuery::transExecute( SqlConnection& sqlConn, SuccessCallback& transSuccess )
{
	SqlOperation::transExecute(sqlConn,transSuccess);
	//the callback should now be primed with the result (or lack of)
	//on complete transaction success, we will push it to the queue
	transSuccess = [&]() { _queue->push(_callback); };
}

void SqlResultQueue::processCallbacks()
{
	//execute the callbacks waiting in the synchronization queue
	QueryCallback callMe;
	while (this->try_pop(callMe))
		callMe.invoke();
}
