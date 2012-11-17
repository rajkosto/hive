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

#include <Poco/Mutex.h>
#include <Poco/Thread.h>
#include <Poco/AtomicCounter.h>
#include <Poco/ThreadLocal.h>

#include <boost/unordered_map.hpp>

#include "Database/Database.h"
#include "Database/SqlStatement.h"
#include "SqlDelayThread.h"

class SqlTransaction;
class SqlResultQueue;
class SqlStmtParameters;
class SqlParamBinder;

class ConcreteDatabase : public Database
{
public:
	virtual ~ConcreteDatabase();

	bool Initialize(class Poco::Logger& dbLogger, const std::string& infoString, bool logSql, const std::string& logDir, int maxPingTime, int nConns) override;
	//start worker thread for async DB request execution
	void InitDelayThread() override;
	//stop worker thread
	void HaltDelayThread() override;

	//Logging
	Poco::Logger& CreateLogger(const std::string& subName) override;
	void SetLoggerLevel(int newLevel) override;

	/// Synchronous DB queries
	QueryResult* Query(const char* sql) override;

	QueryNamedResult* QueryNamed(const char* sql) override;

	QueryResult* PQuery(const char* format,...) override;
	QueryNamedResult* PQueryNamed(const char* format,...) override;

	bool DirectExecute(const char* sql) override;
	bool DirectPExecute(const char* format,...) override;

	bool Execute(const char* sql) override;
	bool PExecute(const char* format,...) override;

	bool AsyncQuery(QueryCallback::FuncType func, const char* sql) override;
	bool AsyncPQuery(QueryCallback::FuncType func, const char* format, ...) override;

	// Writes SQL commands to a LOG file
	bool PExecuteLog(const char* format,...) override;

	bool BeginTransaction() override;
	bool CommitTransaction() override;
	bool RollbackTransaction() override;
	//for sync transaction execution
	bool CommitTransactionDirect() override;

	//PREPARED STATEMENT API

	//allocate index for prepared statement with SQL request 'fmt'
	SqlStatement* CreateStatement(SqlStatementID& index, const std::string& fmt) override;
	//get prepared statement format string
	std::string GetStmtString(UInt32 stmtId) const override;

	operator bool () const override { return (m_pQueryConnections.size() && m_pAsyncConn != 0); }

	//escape string generation
	std::string escape_string(std::string str) override;

	// must be called before first query in thread (one time for thread using one from existing Database objects)
	void ThreadStart() override;
	// must be called before finish thread run (one time for thread using one from existing Database objects)
	void ThreadEnd() override;

	// set database-wide result queue. also we should use object-bases and not thread-based result queues
	void ProcessResultQueue() override;

	UInt32 GetPingInterval() const override { return m_pingIntervallms; }

	//function to ping database connections
	void Ping() override;

	//set this to allow async transactions
	//you should call it explicitly after your server successfully started up
	//NO ASYNC TRANSACTIONS DURING SERVER STARTUP - ONLY DURING RUNTIME!!!
	void AllowAsyncTransactions() override { m_bAllowAsyncTransactions = true; }

protected:
	ConcreteDatabase();

	bool CheckFmtError(int res, const char* format) const;
	bool DoDelay(const char* sql, QueryCallback callback);

	void StopServer();

	//factory method to create SqlConnection objects
	virtual SqlConnection* CreateConnection() = 0;
	//factory method to create SqlDelayThread objects
	virtual SqlDelayThread* CreateDelayThread();

	class TransHelper
	{
	public:
		TransHelper() : m_pTrans(NULL) {}
		~TransHelper();

		//initializes new SqlTransaction object
		SqlTransaction* init();
		//gets pointer on current transaction object. Returns NULL if transaction was not initiated
		SqlTransaction* get() const { return m_pTrans; }
		//detaches SqlTransaction object allocated by init() function
		//next call to get() function will return NULL!
		//do not forget to destroy obtained SqlTransaction object!
		SqlTransaction* detach();
		//destroyes SqlTransaction allocated by init() function
		void reset();

	private:
		SqlTransaction * m_pTrans;
	};

	//per-thread based storage for SqlTransaction object initialization - no locking is required
	typedef Poco::ThreadLocal<TransHelper> DBTransHelperTSS;
	DBTransHelperTSS m_TransStorage;

	///< DB connections

	//round-robin connection selection
	SqlConnection* getQueryConnection();
	//for now return one single connection for async requests
	SqlConnection* getAsyncConnection() const { return m_pAsyncConn; }

	friend class SqlStatementImpl;
	//PREPARED STATEMENT API
	//query function for prepared statements
	bool ExecuteStmt(const SqlStatementID& id, SqlStmtParameters* params);
	bool DirectExecuteStmt(const SqlStatementID& id, SqlStmtParameters* params);

	//connection helper counters
	int m_nQueryConnPoolSize;             //current size of query connection pool
	Poco::AtomicCounter m_nQueryCounter;  //counter for connection selection

	//lets use pool of connections for sync queries
	typedef std::vector<SqlConnection*> SqlConnectionContainer;
	SqlConnectionContainer m_pQueryConnections;

	//only one single DB connection for transactions
	SqlConnection* m_pAsyncConn;

	SqlResultQueue*	m_pResultQueue;			///< Transaction queues from diff. threads
	SqlDelayThread*	m_threadBody;			///< Pointer to delay sql executer (owned by m_delayThread)
	Poco::Thread*	m_delayThread;			///< Pointer to executer thread

	bool m_bAllowAsyncTransactions;			///< flag which specifies if async transactions are enabled

	//PREPARED STATEMENT REGISTRY
	class PreparedStmtRegistry
	{
	public:
		//find existing or add a new record in registry
		UInt32 getStmtId(const std::string& fmt);
		std::string getStmtString(UInt32 stmtId) const;
	private:
		typedef Poco::FastMutex RegistryLockType;
		typedef Poco::ScopedLock<RegistryLockType> RegistryGuardType;
		mutable RegistryLockType _lock; //guards _map

		typedef boost::unordered_map<std::string, UInt32> StatementMap;
		StatementMap _map;
	} m_prepStmtRegistry;

	class Poco::Logger* _logger;
private:
	bool m_logSQL;
	std::string m_logsDir;
	UInt32 m_pingIntervallms;
};