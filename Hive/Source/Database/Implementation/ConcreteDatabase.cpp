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

ConcreteDatabase::ConcreteDatabase() : m_pAsyncConn(NULL), m_pResultQueue(NULL), m_threadBody(NULL), m_delayThread(NULL),
	m_logSQL(false), m_pingIntervallms(0), m_nQueryConnPoolSize(1), m_bAllowAsyncTransactions(false), _logger(nullptr)
{
	m_nQueryCounter = -1;
}

ConcreteDatabase::~ConcreteDatabase()
{
	StopServer();
}

#include <boost/lexical_cast.hpp>

bool ConcreteDatabase::Initialize(Poco::Logger& dbLogger, const std::string& infoString, bool logSql, const std::string& logDir, int maxPingTime, int nConns)
{
	_logger = &dbLogger;

	// Enable logging of SQL commands (usually only high-risk commands)
	// (See method: PExecuteLog)
	m_logSQL = logSql;
	m_logsDir = logDir;
	if(!m_logsDir.empty())
	{
		if((m_logsDir.at(m_logsDir.length()-1)!='/') && (m_logsDir.at(m_logsDir.length()-1)!='\\'))
			m_logsDir.append("/");
	}

	m_pingIntervallms = maxPingTime * (60*60 * 1000);

	//create DB connections

	//setup connection pool size
	if(nConns < MIN_CONNECTION_POOL_SIZE)
		m_nQueryConnPoolSize = MIN_CONNECTION_POOL_SIZE;
	else if(nConns > MAX_CONNECTION_POOL_SIZE)
		m_nQueryConnPoolSize = MAX_CONNECTION_POOL_SIZE;
	else
		m_nQueryConnPoolSize = nConns;

	//create connection pool for sync requests
	for (int i = 0; i < m_nQueryConnPoolSize; ++i)
	{
		SqlConnection* pConn = CreateConnection();
		if(!pConn->Initialize(infoString))
		{
			delete pConn;
			return false;
		}

		m_pQueryConnections.push_back(pConn);
	}

	//create and initialize connection for async requests
	m_pAsyncConn = CreateConnection();
	if(!m_pAsyncConn->Initialize(infoString))
		return false;

	m_pResultQueue = new SqlResultQueue;

	InitDelayThread();
	return true;
}

void ConcreteDatabase::StopServer()
{
	HaltDelayThread();
	/*Delete objects*/
	if(m_pResultQueue)
	{
		delete m_pResultQueue;
		m_pResultQueue = NULL;
	}

	if(m_pAsyncConn)
	{
		delete m_pAsyncConn;
		m_pAsyncConn = NULL;
	}

	for (size_t i = 0; i < m_pQueryConnections.size(); ++i)
		delete m_pQueryConnections[i];

	m_pQueryConnections.clear();

}

SqlDelayThread* ConcreteDatabase::CreateDelayThread()
{
	assert(m_pAsyncConn);
	return new SqlDelayThread(this, m_pAsyncConn);
}

#include <Poco/Thread.h>

void ConcreteDatabase::InitDelayThread()
{
	assert(!m_delayThread);

	//New delay thread for delay execute
	m_threadBody = CreateDelayThread();              // will deleted at m_delayThread delete
	m_delayThread = new Poco::Thread("SQL Delay Thread");
	m_delayThread->start(*m_threadBody);
}

void ConcreteDatabase::HaltDelayThread()
{
	if (!m_threadBody || !m_delayThread) return;

	m_threadBody->Stop();                                   //Stop event
	m_delayThread->join();                                  //Wait for flush to DB
	delete m_delayThread;                                   //This also deletes m_threadBody
	m_delayThread = NULL;
	m_threadBody = NULL;
}

Poco::Logger& ConcreteDatabase::CreateLogger(const std::string& subName)
{
	string newLoggerName = _logger->name() + "." + subName;
	Poco::Logger& theLogger = Poco::Logger::get(newLoggerName);
	theLogger.setChannel(_logger->getChannel());
	theLogger.setLevel(_logger->getLevel());
	return theLogger;
}

void ConcreteDatabase::SetLoggerLevel( int newLevel )
{
	poco_assert(_logger != nullptr);
	Poco::Logger::setLevel(_logger->name(),newLevel);
}

void ConcreteDatabase::ThreadStart()
{
}

void ConcreteDatabase::ThreadEnd()
{
}

QueryResult* ConcreteDatabase::Query( const char* sql )
{
	SqlConnection::Lock guard(getQueryConnection());
	return guard->Query(sql);
}

QueryNamedResult* ConcreteDatabase::QueryNamed( const char* sql )
{
	SqlConnection::Lock guard(getQueryConnection());
	return guard->QueryNamed(sql);
}

bool ConcreteDatabase::DirectExecute( const char* sql )
{
	if(!m_pAsyncConn)
		return false;

	SqlConnection::Lock guard(m_pAsyncConn);
	return guard->Execute(sql);
}

void ConcreteDatabase::ProcessResultQueue()
{
	if(m_pResultQueue)
		m_pResultQueue->Update();
}

std::string ConcreteDatabase::escape_string(std::string str)
{
	if(str.empty())
		return str;

	char* buf = new char[str.size()*2+1];
	//we don't care what connection to use - escape string will be the same
	m_pQueryConnections[0]->escape_string(buf,str.c_str(),str.length());
	str = buf;
	delete[] buf;
	return str;
}

SqlConnection* ConcreteDatabase::getQueryConnection()
{
	int nCount = 0;

	if(m_nQueryCounter == long(1 << 31))
		m_nQueryCounter = 0;
	else
		nCount = ++m_nQueryCounter;

	return m_pQueryConnections[nCount % m_nQueryConnPoolSize];
}

void ConcreteDatabase::Ping()
{
	const char* sql = "SELECT 1";

	{
		SqlConnection::Lock guard(m_pAsyncConn);
		delete guard->Query(sql);
	}

	for (int i = 0; i < m_nQueryConnPoolSize; ++i)
	{
		SqlConnection::Lock guard(m_pQueryConnections[i]);
		delete guard->Query(sql);
	}
}

#include <Poco/LocalDateTime.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Format.h>

namespace { const size_t MAX_QUERY_LEN = 32*1024; };

bool ConcreteDatabase::PExecuteLog(const char* format,...)
{
	if (!format)
		return false;

	va_list ap;
	char szQuery[MAX_QUERY_LEN];
	va_start(ap, format);
	int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap );
	va_end(ap);

	if (!CheckFmtError(res,format))
		return false;

	if( m_logSQL )
	{
		string fName(Poco::DateTimeFormatter::format(Poco::LocalDateTime(), "%Y-%m-%d_logSQL.sql")); 

		string logsDir_fname = m_logsDir+fName;
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

	return Execute(szQuery);
}

QueryResult* ConcreteDatabase::PQuery(const char* format,...)
{
	if(!format) return NULL;

	va_list ap;
	char szQuery[MAX_QUERY_LEN];
	va_start(ap, format);
	int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap );
	va_end(ap);

	if (!CheckFmtError(res,format))
		return false;

	return Query(szQuery);
}

QueryNamedResult* ConcreteDatabase::PQueryNamed(const char* format,...)
{
	if(!format) return NULL;

	va_list ap;
	char szQuery[MAX_QUERY_LEN];
	va_start(ap, format);
	int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap );
	va_end(ap);

	if (!CheckFmtError(res,format))
		return false;

	return QueryNamed(szQuery);
}

bool ConcreteDatabase::Execute(const char* sql)
{
	if (!m_pAsyncConn)
		return false;

	SqlTransaction* pTrans = m_TransStorage->get();
	if(pTrans)
	{
		//add SQL request to trans queue
		pTrans->DelayExecute(new SqlPlainRequest(sql));
	}
	else
	{
		//if async execution is not available
		if(!m_bAllowAsyncTransactions)
			return DirectExecute(sql);

		// Simple sql statement
		m_threadBody->Delay(new SqlPlainRequest(sql));
	}

	return true;
}

bool ConcreteDatabase::PExecute(const char* format,...)
{
	if (!format)
		return false;

	va_list ap;
	char szQuery[MAX_QUERY_LEN];
	va_start(ap, format);
	int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap );
	va_end(ap);

	if (!CheckFmtError(res,format))
		return false;

	return Execute(szQuery);
}

bool ConcreteDatabase::DirectPExecute(const char* format,...)
{
	if (!format)
		return false;

	va_list ap;
	char szQuery[MAX_QUERY_LEN];
	va_start(ap, format);
	int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap );
	va_end(ap);

	if (!CheckFmtError(res,format))
		return false;

	return DirectExecute(szQuery);
}

bool ConcreteDatabase::AsyncQuery( QueryCallback::FuncType func, const char* sql )
{
	if (!sql)
		return false;

	return this->DoDelay(sql, QueryCallback(func));
}

bool ConcreteDatabase::AsyncPQuery( QueryCallback::FuncType func, const char* format, ... )
{
	if(!format) return false;
	char szQuery[MAX_QUERY_LEN];
	{		
		va_list ap;
		va_start(ap, format);
		int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap );
		va_end(ap);

		if (!CheckFmtError(res,format))
			return false;
	}
	return AsyncQuery(func, szQuery);
}

bool ConcreteDatabase::BeginTransaction()
{
	if (!m_pAsyncConn)
		return false;

	//initiate transaction on current thread
	//currently we do not support queued transactions
	m_TransStorage->init();
	return true;
}

bool ConcreteDatabase::CommitTransaction()
{
	if (!m_pAsyncConn)
		return false;

	//check if we have pending transaction
	if(!m_TransStorage->get())
		return false;

	//if async execution is not available
	if(!m_bAllowAsyncTransactions)
		return CommitTransactionDirect();

	//add SqlTransaction to the async queue
	m_threadBody->Delay(m_TransStorage->detach());
	return true;
}

bool ConcreteDatabase::CommitTransactionDirect()
{
	if (!m_pAsyncConn)
		return false;

	//check if we have pending transaction
	if(!m_TransStorage->get())
		return false;

	//directly execute SqlTransaction
	SqlTransaction* pTrans = m_TransStorage->detach();
	pTrans->Execute(m_pAsyncConn);
	delete pTrans;

	return true;
}

bool ConcreteDatabase::RollbackTransaction()
{
	if (!m_pAsyncConn)
		return false;

	if(!m_TransStorage->get())
		return false;

	//remove scheduled transaction
	m_TransStorage->reset();

	return true;
}

bool ConcreteDatabase::ExecuteStmt( const SqlStatementID& id, SqlStmtParameters* params )
{
	if (!m_pAsyncConn)
		return false;

	SqlTransaction *pTrans = m_TransStorage->get();
	if(pTrans)
	{
		//add SQL request to trans queue
		pTrans->DelayExecute(new SqlPreparedRequest(id, params));
	}
	else
	{
		//if async execution is not available
		if(!m_bAllowAsyncTransactions)
			return DirectExecuteStmt(id, params);

		// Simple sql statement
		m_threadBody->Delay(new SqlPreparedRequest(id, params));
	}

	return true;
}

bool ConcreteDatabase::DirectExecuteStmt( const SqlStatementID& id, SqlStmtParameters* params )
{
	poco_assert(params);
	std::auto_ptr<SqlStmtParameters> p(params);
	//execute statement
	SqlConnection::Lock _guard(getAsyncConnection());
	return _guard->ExecuteStmt(id, *params);
}

SqlStatement* ConcreteDatabase::CreateStatement(SqlStatementID& index, const std::string& fmt )
{
	//initialize the statement if its not
	if(!index.isInitialized())
	{
		//count input parameters
		size_t nParams = std::count(fmt.begin(), fmt.end(), '?');
		UInt32 nId = m_prepStmtRegistry.getStmtId(fmt);

		//save initialized statement index info
		index.init(nId, nParams);
	}

	return new SqlStatementImpl(index, *this);
}

std::string ConcreteDatabase::GetStmtString(UInt32 stmtId) const
{
	return m_prepStmtRegistry.getStmtString(stmtId);
}

bool ConcreteDatabase::DoDelay( const char* sql, QueryCallback callback )
{
	return m_threadBody->Delay(new SqlQuery(sql, callback, m_pResultQueue));
}

bool ConcreteDatabase::CheckFmtError( int res, const char* format ) const
{
	if(res==-1)
	{
		_logger->error(Poco::format("SQL Query truncated (and not executed) for format: %s",string(format)));
		return false;
	}
	return true;
}

//HELPER CLASSES AND FUNCTIONS
ConcreteDatabase::TransHelper::~TransHelper()
{
	reset();
}

SqlTransaction* ConcreteDatabase::TransHelper::init()
{
	poco_assert(!m_pTrans);   //if we will get a nested transaction request - we MUST fix code!!!
	m_pTrans = new SqlTransaction;
	return m_pTrans;
}

SqlTransaction* ConcreteDatabase::TransHelper::detach()
{
	SqlTransaction * pRes = m_pTrans;
	m_pTrans = NULL;
	return pRes;
}

void ConcreteDatabase::TransHelper::reset()
{
	if(m_pTrans)
	{
		delete m_pTrans;
		m_pTrans = NULL;
	}
}

namespace
{
	UInt32 GetUniqueId()
	{
		UInt64 cpuTimer = __rdtsc();
		UInt32 mixed = cpuTimer & 0xFFFFFFFF;
		mixed ^= (UInt64(cpuTimer >> 32)) & 0xFFFFFFFF;
		return mixed;
	}
};

UInt32 ConcreteDatabase::PreparedStmtRegistry::getStmtId( const std::string& fmt )
{
	UInt32 nId;

	RegistryGuardType _guard(_lock);
	StatementMap::const_iterator iter = _map.find(fmt);
	if(iter == _map.end())
	{
		nId = GetUniqueId();
		_map[fmt] = nId;
	}
	else
		nId = iter->second;

	return nId;
}

std::string ConcreteDatabase::PreparedStmtRegistry::getStmtString( UInt32 stmtId ) const
{
	if(stmtId == 0)
		return std::string();

	RegistryGuardType _guard(_lock);
	for(StatementMap::const_iterator iter = _map.begin(); iter != _map.end(); ++iter)
	{
		if(iter->second == stmtId)
			return iter->first;
	}

	return std::string();
}
