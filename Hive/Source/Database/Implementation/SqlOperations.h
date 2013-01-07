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

#include <boost/ptr_container/ptr_vector.hpp>
#include <tbb/concurrent_queue.h>

// ---- BASE ---

class Database;
class SqlConnection;
class SqlDelayThread;
class SqlStmtParameters;

class SqlOperation
{
public:
	virtual void onRemove() { delete this; }
	bool execute(SqlConnection& sqlConn);
	virtual ~SqlOperation() {}
protected:
	friend class SqlTransaction;
	//execute as a single thing
	virtual bool rawExecute(SqlConnection& sqlConn, bool throwExc = false) = 0;
	//execute as part of a transaction (no retries)
	typedef boost::function<void()> SuccessCallback;
	virtual void transExecute(SqlConnection& sqlConn, SuccessCallback& transSuccess)
	{
		//execute normally, but throw exc on error so we dont retry
		this->rawExecute(sqlConn,true);
	}
};

// ---- ASYNC STATEMENTS / TRANSACTIONS ----

class SqlPlainRequest : public SqlOperation
{
public:
	SqlPlainRequest(std::string sql) : _sql(std::move(sql)) {};
	~SqlPlainRequest() {};
protected:
	bool rawExecute(SqlConnection& sqlConn, bool throwExc) override;
private:
	std::string _sql;
};

class SqlTransaction : public SqlOperation
{
public:
	SqlTransaction() {}
	~SqlTransaction() {};

	void queueOperation(SqlOperation* sql) { _queue.push_back(sql); }
protected:
	bool rawExecute(SqlConnection& sqlConn, bool throwExc) override;
private:
	boost::ptr_vector<SqlOperation> _queue;
};

class SqlPreparedRequest : public SqlOperation
{
public:
	SqlPreparedRequest(const SqlStatementID& stId, SqlStmtParameters& arg) : _id(stId) { _params.swap(arg); }
	~SqlPreparedRequest() {}
protected:
	bool rawExecute(SqlConnection& sqlConn, bool throwExc) override;
private:
	SqlStatementID _id;
	SqlStmtParameters _params;
};

// ---- ASYNC QUERIES ----

class SqlQuery;			//contains a single async query
class QueryResult;		//the result of one
class SqlResultQueue;	//queue for thread sync

class SqlResultQueue : public tbb::concurrent_queue<QueryCallback>
{
public:
	SqlResultQueue() {}
	void processCallbacks();
};

class SqlQuery : public SqlOperation
{
public:
	SqlQuery(std::string sql, QueryCallback callback, SqlResultQueue& queue) : _sql(std::move(sql)), _callback(callback), _queue(&queue) {};
	~SqlQuery() {};
protected:
	bool rawExecute(SqlConnection& sqlConn, bool throwExc) override;
	void transExecute(SqlConnection& sqlConn, SuccessCallback& transSuccess) override;
private:
	std::string _sql;
	QueryCallback _callback;
	SqlResultQueue* _queue;
};
