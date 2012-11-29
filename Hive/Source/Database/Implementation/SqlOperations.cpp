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

bool SqlPlainRequest::rawExecute(SqlConnection& sqlConn)
{
	//just do it
	return Retry::SqlOp<bool>(sqlConn.getDB().getLogger(),[&](SqlConnection& c){ return c.execute(_sql.c_str()); })(sqlConn,"PlainRequest",[&](){ return _sql; });
}

SqlTransaction::~SqlTransaction()
{
	while(!_queue.empty())
		_queue.pop_back();
}

bool SqlTransaction::rawExecute(SqlConnection& sqlConn)
{
	if(_queue.empty())
		return true;

	sqlConn.transactionStart();

	const size_t nItems = _queue.size();
	for (size_t i=0; i<nItems; i++)
	{
		SqlOperation& stmt = _queue[i];

		if(!stmt.rawExecute(sqlConn))
		{
			sqlConn.transactionRollback();
			return false;
		}
	}

	return sqlConn.transactionCommit();
}

bool SqlPreparedRequest::rawExecute(SqlConnection& sqlConn)
{
	return Retry::SqlOp<bool>(sqlConn.getDB().getLogger(),[&](SqlConnection& c){ return c.executeStmt(_id, _params); })(sqlConn,"PreparedRequest",[&](){ return sqlConn.getStmt(_id)->getSqlString(true); });
}

// ---- ASYNC QUERIES ----
bool SqlQuery::rawExecute(SqlConnection& sqlConn)
{
	if(!_queue)
		return false;

	//execute the query and store the result in the callback
	auto res = Retry::SqlOp< unique_ptr<QueryResult> >(sqlConn.getDB().getLogger(),[&](SqlConnection& c){ return c.query(_sql.c_str()); })(sqlConn);
	_callback.setResult(res.release());
	//add the callback to the sql result queue of the thread it originated from
	_queue->push(_callback);

	return true;
}

void SqlResultQueue::processCallbacks()
{
	//execute the callbacks waiting in the synchronization queue
	QueryCallback callMe;
	while (this->try_pop(callMe))
		callMe.invoke();
}
