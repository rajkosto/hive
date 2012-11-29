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
#include <boost/ptr_container/ptr_vector.hpp>

#include "Database/Database.h"
#include "Database/SqlStatement.h"
#include "SqlDelayThread.h"
#include "SqlOperations.h"

class SqlParamBinder;

class ConcreteDatabase : public Database
{
public:
	virtual ~ConcreteDatabase();

	bool initialise(Poco::Logger& dbLogger, const std::string& infoString, bool logSql, const std::string& logDir, size_t nConns) override;
	
	void initDelayThread() override;
	void haltDelayThread() override;

	Poco::Logger& getLogger() { return *_logger; }

	unique_ptr<QueryResult> query(const char* sql) override;
	unique_ptr<QueryNamedResult> namedQuery(const char* sql) override;

	unique_ptr<QueryResult> queryParams(const char* format,...) override;
	unique_ptr<QueryNamedResult> namedQueryParams(const char* format,...) override;

	bool directExecute(const char* sql) override;
	bool directExecuteParams(const char* format,...) override;

	bool execute(const char* sql) override;
	bool executeParams(const char* format,...) override;

	bool asyncQuery(QueryCallback::FuncType func, const char* sql) override;
	bool asyncQueryParams(QueryCallback::FuncType func, const char* format, ...) override;

	bool executeParamsLog(const char* format,...) override;

	bool transactionStart() override;
	bool transactionCommit() override;
	bool transactionRollback() override;
	bool transactionCommitDirect() override;

	unique_ptr<SqlStatement> makeStatement(SqlStatementID& index, std::string sqlText) override;
	const char* getStmtString(UInt32 stmtId) const;

	operator bool () const override { return (_queryConns.size() && _asyncConn); }

	std::string escape(const std::string& str) const override;

	void threadEnter() override;
	void threadExit() override;

	void invokeCallbacks() override;

	bool checkConnections() override;

	//Call this once you're out of global constructor code/DLLMain
	void allowAsyncOperations() override { _asyncAllowed = true; }
protected:
	ConcreteDatabase();

	bool checkFmtError(int res, const char* format) const;
	bool doDelay(const char* sql, QueryCallback callback);

	void stopServer();

	//factory method to create SqlConnection objects
	virtual unique_ptr<SqlConnection> createConnection(const std::string& infoString) = 0;
	//factory method to create SqlDelayThread objects
	virtual unique_ptr<SqlDelayThread> createDelayThread();

	class TransHelper
	{
	public:
		TransHelper() : _trans(nullptr) {}
		~TransHelper() {}

		//initializes new SqlTransaction object
		SqlTransaction* init();
		//gets pointer on current transaction object. Returns NULL if transaction was not initiated
		SqlTransaction* get() const { return _trans.get(); }
		//detaches SqlTransaction object allocated by init() function
		//next call to get() function will return NULL!
		//do not forget to destroy obtained SqlTransaction object!
		SqlTransaction* detach() { return _trans.release(); }
		//destroys SqlTransaction allocated by init() function
		void reset() { _trans.reset(); }

	private:
		unique_ptr<SqlTransaction> _trans;
	};

	//per-thread based storage for SqlTransaction object initialization - no locking is required
	typedef Poco::ThreadLocal<TransHelper> DBTransHelperTSS;
	DBTransHelperTSS _transStorage;

	//DB connections

	//round-robin connection selection
	SqlConnection& getQueryConnection();
	//for now return one single connection for async requests
	SqlConnection& getAsyncConnection();

	friend class SqlStatementImpl;
	//PREPARED STATEMENT API
	//query function for prepared statements
	bool executeStmt(const SqlStatementID& id, SqlStmtParameters& params);
	bool directExecuteStmt(const SqlStatementID& id, SqlStmtParameters& params);

	//connection helper counters
	Poco::AtomicCounter _currConn;  //counter for connection selection

	//pool of connections for general queries
	typedef boost::ptr_vector<SqlConnection> SqlConnectionContainer;
	SqlConnectionContainer _queryConns;

	//only one single DB connection for transactions
	unique_ptr<SqlConnection> _asyncConn;

	//Transaction queues from diff. threads
	SqlResultQueue _resultQueue;

	class DelayThreadRunnable : public Poco::Thread
	{
	public:
		DelayThreadRunnable(unique_ptr<SqlDelayThread> body) 
			: Poco::Thread("SQL Delay Thread"), _body(std::move(body)) {} 
		void start() { Poco::Thread::start(*_body); }
		void stop() 
		{
			_body->stop();			//send stop signal
			Poco::Thread::join();	//wait for thread to finish
		}
		bool queueOperation(SqlOperation* sql) { return _body->queueOperation(sql); }
	private:
		unique_ptr<SqlDelayThread> _body;
	};
	unique_ptr<DelayThreadRunnable>	_delayRunner;

	//To prevent threading before they work properly
	bool _asyncAllowed;

	//PREPARED STATEMENT REGISTRY
	class PreparedStmtRegistry
	{
	public:
		PreparedStmtRegistry() : _nextId(0) {}

		//manually insert a new record
		void insertStmt(UInt32 theId, std::string fmt);
		//find existing or add a new record in registry
		UInt32 getStmtId(std::string fmt);
		//get sql for id
		const char* getStmtString(UInt32 stmtId) const;
		//is id defined ?
		bool idDefined(UInt32 theId) const { return (_idMap.count(theId) > 0); }
	private:
		void _insertStmt(UInt32 theId, std::string fmt);

		typedef Poco::FastMutex RegistryLockType;
		typedef Poco::ScopedLock<RegistryLockType> RegistryGuardType;
		mutable RegistryLockType _lock; //guards _map

		typedef boost::unordered_map<std::string, UInt32> StatementMap;
		StatementMap _stringMap;
		typedef std::map<UInt32,const char*> IdMap;
		IdMap _idMap;

		UInt32 _nextId;
	} _prepStmtRegistry;

	Poco::Logger* _logger;
private:
	ConcreteDatabase(const ConcreteDatabase&);
	ConcreteDatabase& operator = (const ConcreteDatabase&);

	bool _shouldLogSQL;
	std::string _sqlLogsDir;
};