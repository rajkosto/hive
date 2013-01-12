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

#include <Poco/Logger.h>

#include "QueryResult.h"
#include "SqlStatement.h"
#include "Callback.h"

namespace Poco { class Logger; };

class Database : public boost::noncopyable
{
public:
	virtual ~Database() {};

	typedef std::map<string,string> KeyValueColl;
	virtual bool initialise(Poco::Logger& dbLogger, const KeyValueColl& connParams, bool logSql = false, const std::string& logDir = "", size_t nConns = 1) = 0;

	//start worker thread for async DB request execution
	virtual void initDelayThread() = 0;
	//stop worker thread
	virtual void haltDelayThread() = 0;

	//Synchronous DB queries
	virtual unique_ptr<QueryResult> query(const char* sql) = 0;
	virtual unique_ptr<QueryNamedResult> namedQuery(const char* sql) = 0;

	virtual unique_ptr<QueryResult> queryParams(const char* format,...) = 0;
	virtual unique_ptr<QueryNamedResult> namedQueryParams(const char* format,...) = 0;

	virtual bool directExecute(const char* sql) = 0;
	virtual bool directExecuteParams(const char* format,...) = 0;

	//Query creation helpers
	virtual std::string sqlLike() const = 0;
	virtual std::string sqlTableSim(const std::string& tableName) const = 0;
	virtual std::string sqlConcat(const std::string& a, const std::string& b, const std::string& c) const = 0;
	virtual std::string sqlOffset() const = 0;

	//Async queries and query holders
	virtual bool asyncQuery(QueryCallback::FuncType func, const char* sql) = 0;
	virtual bool asyncQueryParams(QueryCallback::FuncType func, const char* format, ...) = 0;

	virtual bool execute(const char* sql) = 0;
	virtual bool executeParams(const char* format,...) = 0;

	//Writes SQL commands to a LOG file
	virtual bool executeParamsLog(const char* format,...) = 0;

	virtual bool transactionStart() = 0;
	virtual bool transactionCommit() = 0;
	virtual bool transactionRollback() = 0;
	//for sync transaction execution
	virtual bool transactionCommitDirect() = 0;

	//PREPARED STATEMENT API

	//allocate index for prepared statement with SQL request string
	virtual unique_ptr<SqlStatement> makeStatement(SqlStatementID& index, std::string sqlText) = 0;

	//Is DB ready for requests
	virtual operator bool () const = 0;

	//Escape string generation
	virtual std::string escape(const std::string& str) const = 0;

	//Call before first query in a thread
	virtual void threadEnter() = 0;
	//Must be called before the thread that called ThreadStart exits
	virtual void threadExit() = 0;

	//Invoke callbacks for finished async queries
	virtual void invokeCallbacks() = 0;

	//Check if connection to DB is alive and well
	virtual bool checkConnections() = 0;

	//Call this once you're out of global constructor code/DLLMain
	virtual void allowAsyncOperations() = 0;
};