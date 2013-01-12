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
#include "Shared/Common/Exception.h"

#include <boost/noncopyable.hpp>
#include <Poco/Mutex.h>

#include "SqlPreparedStatement.h"

class Database;
class ConcreteDatabase;
class QueryResult;
class QueryNamedResult;
class SqlStatementID;
class SqlStmtParameters;

class SqlConnection : public boost::noncopyable
{
public:
	class SqlException : public Poco::Exception
	{
	public:
		SqlException(int code, const std::string& descr, const char* func, bool connLost = false, bool repeatable = false, std::string query = "")
			: Poco::Exception(descr,code), _query(std::move(query)), _function(func), _connLost(connLost), _repeatable(repeatable) {}

		int getCode() const { return this->code(); }
		const std::string getDescr() const { return this->message(); }
		const char* getFunction() const { return _function; }
		const std::string& getQuery() const { return _query; }

		bool isConnLost() const { return _connLost; }
		bool isRepeatable() const { return _repeatable; }

		void toStream(std::ostream& ostr) const;
		std::string toString() const;
		void toLog(Poco::Logger& logger) const;
	private:
		std::string _query;
		const char* _function;
		bool _connLost;
		bool _repeatable;
	};
	
	virtual ~SqlConnection() {}

	//called to make sure we are connected (after it breaks, etc)
	virtual void connect() = 0;

	//public methods for making queries
	virtual unique_ptr<QueryResult> query(const char* sql) = 0;
	virtual unique_ptr<QueryNamedResult> namedQuery(const char* sql);

	//public methods for making requests
	virtual bool execute(const char* sql) = 0;

	//escape string generation
	virtual size_t escapeString(char* to, const char* from, size_t length) const;

	//nothing do if DB doesn't support transactions
	virtual bool transactionStart();
	virtual bool transactionCommit();
	//can't rollback without transaction support
	virtual bool transactionRollback();

	//methods to work with prepared statements
	bool executeStmt(const SqlStatementID& stId, const SqlStmtParameters& id);

	//SqlConnection object lock
	class Lock
	{
	public:
		Lock(SqlConnection& conn) : _lockedConn(conn) { _lockedConn._connLock.lock(); }
		~Lock() { _lockedConn._connLock.unlock(); }

		SqlConnection* operator->() const { return &_lockedConn; }
	private:
		SqlConnection& _lockedConn;
	};

	ConcreteDatabase& getDB() { return *_dbEngine; }
	//allocate and return prepared statement object
	SqlPreparedStatement* getStmt(const SqlStatementID& stId);
protected:
	SqlConnection(ConcreteDatabase& db) : _dbEngine(&db) {}
	ConcreteDatabase* _dbEngine;

	//make connection-specific prepared statement obj
	virtual SqlPreparedStatement* createPreparedStatement(const char* sqlText);
	//clear any state (because of reconnects, etc)
	virtual void clear();

private:
	typedef Poco::FastMutex ConnLockType;
	ConnLockType _connLock;

	class StmtHolder
	{
	public:
		StmtHolder() {}
		~StmtHolder() { clear(); }

		//remove all statements
		void clear();

		SqlPreparedStatement* getPrepStmtObj(UInt32 stmtId) const;
		void insertPrepStmtObj(UInt32 stmtId, SqlPreparedStatement* stmtObj);
		unique_ptr<SqlPreparedStatement> releasePrepStmtObj(UInt32 stmtId);
	private:
		typedef std::vector< unique_ptr<SqlPreparedStatement> > StatementObjVect;
		StatementObjVect _storage;
	} _stmtHolder;
};