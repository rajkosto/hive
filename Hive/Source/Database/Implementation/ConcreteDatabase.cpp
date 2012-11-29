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

#include "ConcreteDatabase.h"
#include "SqlOperations.h"
#include "SqlConnection.h"
#include "SqlStatementImpl.h"

#include <ctime>
#include <iostream>
#include <fstream>
#include <memory>

static const size_t MIN_CONNECTION_POOL_SIZE = 1;
static const size_t MAX_CONNECTION_POOL_SIZE = 16;

//////////////////////////////////////////////////////////////////////////

ConcreteDatabase::ConcreteDatabase() : _shouldLogSQL(false), _currConn(0), _asyncAllowed(false), _logger(nullptr)
{
}

ConcreteDatabase::~ConcreteDatabase()
{
	stopServer();
}

#include <boost/lexical_cast.hpp>

bool ConcreteDatabase::initialise(Poco::Logger& dbLogger, const string& infoString, bool logSql, const string& logDir, size_t nConns)
{
	stopServer();

	_logger = &dbLogger;

	//Enable logging of SQL commands (usually only high-risk commands)
	//(See method: PExecuteLog)
	_shouldLogSQL = logSql;
	_sqlLogsDir = logDir;
	if(!_sqlLogsDir.empty())
	{
		if((_sqlLogsDir.at(_sqlLogsDir.length()-1)!='/') && (_sqlLogsDir.at(_sqlLogsDir.length()-1)!='\\'))
			_sqlLogsDir.append("/");
	}

	//create DB connections

	//setup connection pool size
	size_t poolSize = nConns;
	if(poolSize < MIN_CONNECTION_POOL_SIZE)
		poolSize = MIN_CONNECTION_POOL_SIZE;
	else if(poolSize > MAX_CONNECTION_POOL_SIZE)
		poolSize = MAX_CONNECTION_POOL_SIZE;

	//initialize and connect all the connections
	_queryConns.clear();
	_queryConns.reserve(poolSize);
	try
	{
		//create and initialize the sync connection pool
		for (size_t i=0; i<poolSize; i++)
		{
			unique_ptr<SqlConnection> pConn = createConnection(infoString);
			pConn->connect();

			_queryConns.push_back(pConn.release());
		}

		//create and initialize connection for async requests
		_asyncConn = createConnection(infoString);
		_asyncConn->connect();
	}
	catch(const SqlConnection::SqlException& e)
	{
		e.toLog(dbLogger);
		return false;
	}

	_resultQueue.clear();

	initDelayThread();
	return true;
}

void ConcreteDatabase::stopServer()
{
	haltDelayThread();

	_resultQueue.clear();
	_asyncConn.reset();
	_queryConns.clear();
}

unique_ptr<SqlDelayThread> ConcreteDatabase::createDelayThread()
{
	poco_assert(_asyncConn);
	return unique_ptr<SqlDelayThread>(new SqlDelayThread(*this, *_asyncConn));
}

#include <Poco/Thread.h>

void ConcreteDatabase::initDelayThread()
{
	if (_delayRunner)
		haltDelayThread();

	poco_assert_dbg(!_delayRunner);

	//New delay thread for delay execute
	_delayRunner.reset(new DelayThreadRunnable(createDelayThread()));
	_delayRunner->start();
}

void ConcreteDatabase::haltDelayThread()
{
	if (!_delayRunner) 
		return;

	_delayRunner->stop();
	_delayRunner.reset();
}

void ConcreteDatabase::threadEnter()
{
}

void ConcreteDatabase::threadExit()
{
}

#include "RetrySqlOp.h"

unique_ptr<QueryResult> ConcreteDatabase::query( const char* sql )
{
	SqlConnection& conn = getQueryConnection();
	SqlConnection::Lock guard(conn);
	return Retry::SqlOp< unique_ptr<QueryResult> >(getLogger(),[sql](SqlConnection& c){ return c.query(sql); })(conn,"Query",[sql](){return sql;});
}

unique_ptr<QueryNamedResult> ConcreteDatabase::namedQuery( const char* sql )
{
	SqlConnection& conn = getQueryConnection();
	SqlConnection::Lock guard(conn);
	return Retry::SqlOp< unique_ptr<QueryNamedResult> >(getLogger(),[sql](SqlConnection& c){ return c.namedQuery(sql); })(conn,"QueryNamed",[sql](){return sql;});
}

bool ConcreteDatabase::directExecute( const char* sql )
{
	if(!_asyncConn)
		return false;

	SqlConnection& conn = getAsyncConnection();
	SqlConnection::Lock guard(conn);
	return Retry::SqlOp<bool>(getLogger(),[sql](SqlConnection& c){ return c.execute(sql); })(conn,"SqlExec",[sql](){return sql;} );
}

void ConcreteDatabase::invokeCallbacks()
{
	_resultQueue.processCallbacks();
}

std::string ConcreteDatabase::escape(const std::string& str) const
{
	if(str.empty())
		return str;

	vector<char> buf(str.size()*2+1);
	//escape string generation is a client-side operation, so the selection of connection is irrelevant
	size_t sLen = _queryConns[0].escapeString(&buf[0],str.c_str(),str.length());

	if (sLen > 0)
		return string(buf.data(),sLen);
	else
		return str;
}

SqlConnection& ConcreteDatabase::getQueryConnection()
{
	poco_assert(_queryConns.size() > 0);
	size_t nCount = _currConn++;
	return _queryConns[nCount % _queryConns.size()];
}

SqlConnection& ConcreteDatabase::getAsyncConnection()
{
	return *_asyncConn;
}

bool ConcreteDatabase::checkConnections()
{
	const char* sql = "SELECT 1";
	auto checkFunc = [](QueryResult* res) -> bool
	{
		if (!res) return false;
		if (!res->fetchRow()) return false;
		if (res->at(0).getInt32() != 1) return false;

		return true;
	};

	//check async conn
	{
		SqlConnection& conn = getAsyncConnection();
		SqlConnection::Lock guard(conn);
		auto qry = Retry::SqlOp< unique_ptr<QueryResult> >(getLogger(),[sql](SqlConnection& c){ return c.query(sql); })(conn,"CheckAsync");
		if (!checkFunc(qry.get()))
			return false;
	}

	//check all sync conns
	for (size_t i=0; i<_queryConns.size(); i++)
	{
		SqlConnection& conn = _queryConns[i];
		SqlConnection::Lock guard(conn);
		auto qry = Retry::SqlOp< unique_ptr<QueryResult> >(getLogger(),[sql](SqlConnection& c){ return c.query(sql); })(conn,"CheckPool");
		if (!checkFunc(qry.get()))
			return false;
	}

	return true;
}

#include <Poco/LocalDateTime.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Format.h>

namespace { const size_t MAX_QUERY_LEN = 32*1024; };

bool ConcreteDatabase::executeParamsLog(const char* format,...)
{
	if (!format)
		return false;

	va_list ap;
	char szQuery[MAX_QUERY_LEN];
	va_start(ap, format);
	int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap );
	va_end(ap);

	if (!checkFmtError(res,format))
		return false;

	if (_shouldLogSQL)
	{
		string fName(Poco::DateTimeFormatter::format(Poco::LocalDateTime(), "%Y-%m-%d_logSQL.sql")); 

		string logsDir_fname = _sqlLogsDir+fName;
		std::ofstream log_file;
		log_file.open(logsDir_fname.c_str(),std::ios::app);
		if (log_file.is_open())
		{
			log_file << szQuery << ";\n";
			log_file.close();
		}
		else
		{
			// The file could not be opened
			_logger->error(Poco::format("SQL-Logging is disabled - Log file for the SQL commands could not be opened: %s",fName));
		}
	}

	return execute(szQuery);
}

unique_ptr<QueryResult> ConcreteDatabase::queryParams(const char* format,...)
{
	if (!format) 
		return nullptr;

	va_list ap;
	char szQuery[MAX_QUERY_LEN];
	va_start(ap, format);
	int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap );
	va_end(ap);

	if (!checkFmtError(res,format))
		return false;

	return query(szQuery);
}

unique_ptr<QueryNamedResult> ConcreteDatabase::namedQueryParams(const char* format,...)
{
	if (!format)
		return nullptr;

	va_list ap;
	char szQuery[MAX_QUERY_LEN];
	va_start(ap, format);
	int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap );
	va_end(ap);

	if (!checkFmtError(res,format))
		return false;

	return namedQuery(szQuery);
}

bool ConcreteDatabase::execute(const char* sql)
{
	if (!_asyncConn)
		return false;

	SqlTransaction* pTrans = _transStorage->get();
	if(pTrans)
	{
		//add SQL request to trans queue
		pTrans->queueOperation(new SqlPlainRequest(sql));
	}
	else
	{
		//if async execution is not available
		if(!_asyncAllowed)
			return directExecute(sql);

		// Simple sql statement
		_delayRunner->queueOperation(new SqlPlainRequest(sql));
	}

	return true;
}

bool ConcreteDatabase::executeParams(const char* format,...)
{
	if (!format)
		return false;

	va_list ap;
	char szQuery[MAX_QUERY_LEN];
	va_start(ap, format);
	int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap );
	va_end(ap);

	if (!checkFmtError(res,format))
		return false;

	return execute(szQuery);
}

bool ConcreteDatabase::directExecuteParams(const char* format,...)
{
	if (!format)
		return false;

	va_list ap;
	char szQuery[MAX_QUERY_LEN];
	va_start(ap, format);
	int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap );
	va_end(ap);

	if (!checkFmtError(res,format))
		return false;

	return directExecute(szQuery);
}

bool ConcreteDatabase::asyncQuery( QueryCallback::FuncType func, const char* sql )
{
	if (!sql)
		return false;

	return this->doDelay(sql, QueryCallback(func));
}

bool ConcreteDatabase::asyncQueryParams( QueryCallback::FuncType func, const char* format, ... )
{
	if (!format)
		return false;

	char szQuery[MAX_QUERY_LEN];
	{		
		va_list ap;
		va_start(ap, format);
		int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap );
		va_end(ap);

		if (!checkFmtError(res,format))
			return false;
	}
	return asyncQuery(func, szQuery);
}

bool ConcreteDatabase::transactionStart()
{
	if (!_asyncConn)
		return false;

	//initiate transaction on current thread
	//currently we do not support queued transactions
	_transStorage->init();
	return true;
}

bool ConcreteDatabase::transactionCommit()
{
	if (!_asyncConn)
		return false;

	//check if we have pending transaction
	if(!_transStorage->get())
		return false;

	//if async execution is not available
	if(!_asyncAllowed)
		return transactionCommitDirect();

	//add SqlTransaction to the async queue
	_delayRunner->queueOperation(_transStorage->detach());
	return true;
}

bool ConcreteDatabase::transactionCommitDirect()
{
	if (!_asyncConn)
		return false;

	//check if we have pending transaction
	if(!_transStorage->get())
		return false;

	//directly execute SqlTransaction
	{
		scoped_ptr<SqlTransaction> pTrans(_transStorage->detach());
		pTrans->execute(getAsyncConnection());	
	}

	return true;
}

bool ConcreteDatabase::transactionRollback()
{
	if (!_asyncConn)
		return false;

	if(!_transStorage->get())
		return false;

	//remove scheduled transaction
	_transStorage->reset();

	return true;
}

bool ConcreteDatabase::executeStmt( const SqlStatementID& id, SqlStmtParameters& params )
{
	if (!_asyncConn)
		return false;

	SqlTransaction *pTrans = _transStorage->get();
	if(pTrans)
	{
		//add SQL request to trans queue
		pTrans->queueOperation(new SqlPreparedRequest(id, params));
	}
	else
	{
		//if async execution is not available
		if(!_asyncAllowed)
			return directExecuteStmt(id, params);

		// Simple sql statement
		_delayRunner->queueOperation(new SqlPreparedRequest(id, params));
	}

	return true;
}

bool ConcreteDatabase::directExecuteStmt( const SqlStatementID& id, SqlStmtParameters& params )
{
	//execute statement
	SqlConnection& conn = getAsyncConnection();
	SqlConnection::Lock guard(conn);

	return Retry::SqlOp<bool>(getLogger(),[&](SqlConnection& c){ return c.executeStmt(id, params); })(conn,"DirectStmtExec",[&](){ return conn.getStmt(id)->getSqlString(true); });
}

unique_ptr<SqlStatement> ConcreteDatabase::makeStatement( SqlStatementID& index, std::string sqlText )
{
	//initialize the statement if its not (or missing in the registry)
	if(!index.isInitialized())
	{
		//count input parameters
		size_t nParams = std::count(sqlText.begin(), sqlText.end(), '?');
		UInt32 nId = _prepStmtRegistry.getStmtId(std::move(sqlText));

		//save initialized statement index info
		index.init(nId, nParams);
	}
	else if (!_prepStmtRegistry.idDefined(index.getId()))
		_prepStmtRegistry.insertStmt(index.getId(),std::move(sqlText));

	return unique_ptr<SqlStatement>(new SqlStatementImpl(index, *this));
}

const char* ConcreteDatabase::getStmtString(UInt32 stmtId) const
{
	return _prepStmtRegistry.getStmtString(stmtId);
}

bool ConcreteDatabase::doDelay( const char* sql, QueryCallback callback )
{
	return _delayRunner->queueOperation(new SqlQuery(sql, callback, _resultQueue));
}

bool ConcreteDatabase::checkFmtError( int res, const char* format ) const
{
	if(res==-1)
	{
		_logger->error(Poco::format("SQL Query truncated (and not executed) for format: %s",string(format)));
		return false;
	}
	return true;
}

//HELPER CLASSES AND FUNCTIONS
SqlTransaction* ConcreteDatabase::TransHelper::init()
{
	//if we need to support nested transaction requests, all this will need to be rewritten
	poco_assert(!_trans);
	_trans.reset(new SqlTransaction());
	return get();
}

void ConcreteDatabase::PreparedStmtRegistry::insertStmt(UInt32 theId, std::string fmt)
{
	RegistryGuardType _guard(_lock);
	_insertStmt(theId,std::move(fmt));
}

void ConcreteDatabase::PreparedStmtRegistry::_insertStmt( UInt32 theId, std::string fmt )
{
	StatementMap::const_iterator it = _stringMap.insert(std::make_pair(std::move(fmt),theId)).first;
	_idMap.insert(std::make_pair(theId,it->first.c_str()));
}

UInt32 ConcreteDatabase::PreparedStmtRegistry::getStmtId( std::string fmt )
{
	UInt32 nId;

	RegistryGuardType _guard(_lock);
	StatementMap::const_iterator iter = _stringMap.find(fmt);
	if(iter == _stringMap.end())
	{
		nId = ++_nextId;
		_insertStmt(nId,fmt);
	}
	else
		nId = iter->second;

	return nId;
}

const char* ConcreteDatabase::PreparedStmtRegistry::getStmtString( UInt32 stmtId ) const
{
	if(stmtId == 0)
		return nullptr;

	RegistryGuardType _guard(_lock);
	IdMap::const_iterator it = _idMap.find(stmtId);
	if (it != _idMap.end())
		return it->second;

	return nullptr;
}
