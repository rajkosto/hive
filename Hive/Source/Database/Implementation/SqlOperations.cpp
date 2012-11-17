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

/// ---- ASYNC STATEMENTS / TRANSACTIONS ----
bool SqlPlainRequest::Execute(SqlConnection* conn)
{
	/// just do it
	SqlConnection::Lock guard(conn);
	return conn->Execute(_sql.c_str());
}

SqlTransaction::~SqlTransaction()
{
	while(!_queue.empty())
	{
		delete _queue.back();
		_queue.pop_back();
	}
}

bool SqlTransaction::Execute(SqlConnection *conn)
{
	if(_queue.empty())
		return true;

	SqlConnection::Lock guard(conn);

	conn->BeginTransaction();

	const int nItems = _queue.size();
	for (int i = 0; i < nItems; ++i)
	{
		SqlOperation * pStmt = _queue[i];

		if(!pStmt->Execute(conn))
		{
			conn->RollbackTransaction();
			return false;
		}
	}

	return conn->CommitTransaction();
}

SqlPreparedRequest::SqlPreparedRequest(const SqlStatementID& stId, SqlStmtParameters* arg) : _id(stId), _params(arg)
{
}

SqlPreparedRequest::~SqlPreparedRequest()
{
	delete _params;
}

bool SqlPreparedRequest::Execute( SqlConnection* conn )
{
	SqlConnection::Lock guard(conn);
	return conn->ExecuteStmt(_id, *_params);
}

/// ---- ASYNC QUERIES ----
bool SqlQuery::Execute(SqlConnection* conn)
{
	if(!_queue)
		return false;

	SqlConnection::Lock guard(conn);
	/// execute the query and store the result in the callback
	QueryResult* res = conn->Query(_sql.c_str());
	_callback.setResult(res);
	/// add the callback to the sql result queue of the thread it originated from
	_queue->push(_callback);

	return true;
}

void SqlResultQueue::Update()
{
	/// execute the callbacks waiting in the synchronization queue
	QueryCallback callMe;
	while (this->try_pop(callMe))
		callMe.execute();
}
