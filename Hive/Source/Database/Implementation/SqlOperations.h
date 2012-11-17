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

#include "Shared/Common/Types.h"
#include "Database/Callback.h"
#include "Database/SqlStatement.h"

#include <tbb/concurrent_queue.h>

/// ---- BASE ---

class Database;
class SqlConnection;
class SqlDelayThread;
class SqlStmtParameters;

class SqlOperation
{
public:
	virtual void OnRemove() { delete this; }
	virtual bool Execute(SqlConnection* conn) = 0;
	virtual ~SqlOperation() {}
};

/// ---- ASYNC STATEMENTS / TRANSACTIONS ----

class SqlPlainRequest : public SqlOperation
{
private:
	std::string _sql;
public:
	SqlPlainRequest(std::string sql) : _sql(sql) {};
	~SqlPlainRequest() {};
	bool Execute(SqlConnection *conn);
};

class SqlTransaction : public SqlOperation
{
private:
	std::vector<SqlOperation*> _queue;

public:
	SqlTransaction() {}
	~SqlTransaction();

	void DelayExecute(SqlOperation* sql)   { _queue.push_back(sql); }
	bool Execute(SqlConnection* conn);
};

class SqlPreparedRequest : public SqlOperation
{
public:
	SqlPreparedRequest(const SqlStatementID& stId, SqlStmtParameters* arg);
	~SqlPreparedRequest();

	bool Execute(SqlConnection* conn);

private:
	SqlStatementID _id;
	SqlStmtParameters* _params;
};

/// ---- ASYNC QUERIES ----

class SqlQuery;                                             /// contains a single async query
class QueryResult;                                          /// the result of one
class SqlResultQueue;                                       /// queue for thread sync

class SqlResultQueue : public tbb::concurrent_queue<QueryCallback>
{
public:
	SqlResultQueue() {}
	void Update();
};

class SqlQuery : public SqlOperation
{
private:
	std::string _sql;
	QueryCallback _callback;
	SqlResultQueue* _queue;
public:
	SqlQuery(std::string sql, QueryCallback callback, SqlResultQueue* queue) : _sql(sql), _callback(callback), _queue(queue) {};
	~SqlQuery() {};
	bool Execute(SqlConnection* conn);
};
