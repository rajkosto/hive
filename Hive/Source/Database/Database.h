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
class Database
{
public:
	virtual ~Database() {};

	virtual bool Initialize(Poco::Logger& dbLogger, const std::string& infoString, bool logSql = false, const std::string& logDir = "", int maxPingTime = 30, int nConns = 1) = 0;
	//start worker thread for async DB request execution
	virtual void InitDelayThread() = 0;
	//stop worker thread
	virtual void HaltDelayThread() = 0;

	//Logging
	virtual Poco::Logger& CreateLogger(const std::string& subName) = 0;
	virtual void SetLoggerLevel(int newLevel) = 0;

	/// Synchronous DB queries
	virtual QueryResult* Query(const char* sql) = 0;

	virtual QueryNamedResult* QueryNamed(const char* sql) = 0;

	virtual QueryResult* PQuery(const char* format,...) = 0;
	virtual QueryNamedResult* PQueryNamed(const char* format,...) = 0;

	virtual bool DirectExecute(const char* sql) = 0;

	virtual bool DirectPExecute(const char* format,...) = 0;

	//Query creation helpers
	virtual std::string like() const = 0;
	virtual std::string table_sim(const std::string& tableName) const = 0;
	virtual std::string concat(const std::string& a, const std::string& b, const std::string& c) const = 0;
	virtual std::string offset() const = 0;

	//Async queries and query holders
	virtual bool AsyncQuery(QueryCallback::FuncType func, const char* sql) = 0;
	virtual bool AsyncPQuery(QueryCallback::FuncType func, const char* format, ...) = 0;

	virtual bool Execute(const char* sql) = 0;
	virtual bool PExecute(const char* format,...) = 0;

	// Writes SQL commands to a LOG file
	virtual bool PExecuteLog(const char* format,...) = 0;

	virtual bool BeginTransaction() = 0;
	virtual bool CommitTransaction() = 0;
	virtual bool RollbackTransaction() = 0;
	//for sync transaction execution
	virtual bool CommitTransactionDirect() = 0;

	//PREPARED STATEMENT API

	//allocate index for prepared statement with SQL request 'fmt'
	virtual SqlStatement* CreateStatement(SqlStatementID& index, const std::string& fmt) = 0;
	//get prepared statement format string
	virtual std::string GetStmtString(const int stmtId) const = 0;

	virtual operator bool () const = 0;

	//escape string generation
	virtual std::string escape_string(std::string str) = 0;

	// must be called before first query in thread (one time for thread using one from existing Database objects)
	virtual void ThreadStart() = 0;
	// must be called before finish thread run (one time for thread using one from existing Database objects)
	virtual void ThreadEnd() = 0;

	// set database-wide result queue. also we should use object-based and not thread-based result queues
	virtual void ProcessResultQueue() = 0;

	virtual UInt32 GetPingInterval() const = 0;

	//function to ping database connections
	virtual void Ping() =0;

	//set this to allow async transactions
	//you should call it explicitly after your server successfully started up
	//NO ASYNC TRANSACTIONS DURING SERVER STARTUP - ONLY DURING RUNTIME!!!
	virtual void AllowAsyncTransactions() = 0;
};