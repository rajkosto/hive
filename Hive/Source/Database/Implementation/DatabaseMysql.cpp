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

#ifdef MYSQL_ENABLED

#include "DatabaseMysql.h"
#include "QueryResultMysql.h"
#include "MySQLDelayThread.h"


#include "Shared/Common/Timer.h"
#include "Shared/Common/Exception.h"

#include <Poco/Logger.h>
#include <Poco/Format.h>
#include <Poco/NumberParser.h>
#include <Poco/String.h>
#include <Poco/StringTokenizer.h>

size_t DatabaseMysql::db_count = 0;

void DatabaseMysql::ThreadStart()
{
	mysql_thread_init();
}

void DatabaseMysql::ThreadEnd()
{
	mysql_thread_end();
}

DatabaseMysql::DatabaseMysql()
{
	// before first connection
	if( db_count++ == 0 )
	{
		// Mysql Library Init
		mysql_library_init(-1, NULL, NULL);

		if (!mysql_thread_safe())
			poco_bugcheck_msg("FATAL ERROR: Used MySQL library isn't thread-safe.");
	}
}

DatabaseMysql::~DatabaseMysql()
{
	StopServer();

	//Free Mysql library pointers for last ~DB
	if(--db_count == 0)
		mysql_library_end();
}


SqlDelayThread* DatabaseMysql::CreateDelayThread()
{
	assert(m_pAsyncConn);
	return new MySQLDelayThread(this, m_pAsyncConn);
}

SqlConnection* DatabaseMysql::CreateConnection()
{
	return new MySQLConnection(*this);
}

std::string DatabaseMysql::like() const 
{
	return "LIKE";
}

std::string DatabaseMysql::table_sim( const std::string& tableName ) const 
{
	char tempBuf[256];
	sprintf(tempBuf,"`%s`",tableName.c_str());
	return string(tempBuf,2+tableName.length());
}

std::string DatabaseMysql::concat( const std::string& a, const std::string& b, const std::string& c ) const 
{
	char tempBuf[512];
	sprintf(tempBuf,"CONCAT( %s , %s , %s )",a.c_str(),b.c_str(),c.c_str());
	return string(tempBuf,16+a.length()+b.length()+c.length());
}

std::string DatabaseMysql::offset() const 
{
	return "LIMIT %d,1";
}

MySQLConnection::MySQLConnection( Database& db ) : SqlConnection(db), mMysql(NULL), _logger(db.CreateLogger("MySQLConnection")) {}

MySQLConnection::~MySQLConnection()
{
	FreePreparedStatements();
	mysql_close(mMysql);
}

bool MySQLConnection::Initialize(const std::string& infoString)
{
	MYSQL* mysqlInit = mysql_init(NULL);
	if (!mysqlInit)
	{
		_logger.error("Could not initialize MySQL connection");
		return false;
	}

	typedef Poco::StringTokenizer Tokens;
	Tokens tokens(infoString,";",Tokens::TOK_TRIM);

	Tokens::Iterator iter = tokens.begin();

	std::string port_or_socket;

	if(iter != tokens.end())
		_host = *iter++;
	if(iter != tokens.end())
		port_or_socket = *iter++;
	if(iter != tokens.end())
		_user = *iter++;
	if(iter != tokens.end())
		_password = *iter++;
	if(iter != tokens.end())
		_database = *iter++;

	//Set charset
	mysql_options(mysqlInit,MYSQL_SET_CHARSET_NAME,"utf8");
	//Disable automatic reconnection, we will do this manually (to re-create broken statements and such)
	my_bool reconnect = 0;
	mysql_options(mysqlInit, MYSQL_OPT_RECONNECT, &reconnect);

#ifdef WIN32
	if(_host==".")                                           // named pipe use option (Windows)
	{
		unsigned int opt = MYSQL_PROTOCOL_PIPE;
		mysql_options(mysqlInit,MYSQL_OPT_PROTOCOL,(char const*)&opt);
		_port = 0;
		_unix_socket.clear();
	}
	else                                                    // generic case
	{
		_port = atoi(port_or_socket.c_str());
		_unix_socket.clear();
	}
#else
	if(_host==".")                                           // socket use option (Unix/Linux)
	{
		unsigned int opt = MYSQL_PROTOCOL_SOCKET;
		mysql_options(mysqlInit,MYSQL_OPT_PROTOCOL,(char const*)&opt);
		_host = "localhost";
		_port = 0;
		_unix_socket = port_or_socket;
	}
	else                                                    // generic case
	{
		_port = atoi(port_or_socket.c_str());
		_unix_socket.clear();
	}
#endif

	return (_Connect(mysqlInit) == 0);
}

int MySQLConnection::_Connect(MYSQL* mysqlInit) 
{
	bool reconnecting = false;
	if (mMysql != NULL) //reconnection attempt
	{
		if (!mysql_ping(mMysql)) //ping ok
			return 0; //already connected
		else
			reconnecting = true;
	}

	for(;;)
	{
		const char* unix_socket = NULL;
		if (_unix_socket.length() > 0)
			unix_socket = _unix_socket.c_str();

		mMysql = mysql_real_connect(mysqlInit, _host.c_str(), _user.c_str(), _password.c_str(), _database.c_str(), _port, unix_socket, 0);
		if (!mMysql)
		{
			string actionToDo = (reconnecting)?string("reconnect"):string("connect");

			_logger.error(Poco::format("Could not "+actionToDo+" to MySQL database at %s: %s",_host,string(mysql_error(mysqlInit))));
			//see if we should fail completely
			{
				const unsigned int ER_DBACCESS_DENIED_ERROR = 1044;
				const unsigned int ER_ACCESS_DENIED_ERROR = 1045;
				unsigned int errNo = mysql_errno(mysqlInit);
				if (errNo == ER_DBACCESS_DENIED_ERROR || errNo == ER_ACCESS_DENIED_ERROR)
				{
					mysql_close(mysqlInit);
					return errNo;
				}
			}
			long sleepTime = 1000;
			_logger.information(Poco::format("Retrying in %d seconds",static_cast<int>(sleepTime/1000)));
			Poco::Thread::sleep(sleepTime);
			continue;
		}
		break;
	}

	string actionDone = (reconnecting)?string("Reconnected"):string("Connected");

	poco_information(_logger,Poco::format(actionDone + " to MySQL database %s:%d/%s client ver: %s server ver: %s",
		_host, _port,_database,
		string(mysql_get_client_info()),
		string(mysql_get_server_info(mMysql))
		));

	/*----------SET AUTOCOMMIT ON---------*/
	// LEAVE 'AUTOCOMMIT' MODE ALWAYS ENABLED!!!
	// WITHOUT IT EVEN 'SELECT' QUERIES WOULD REQUIRE TO BE WRAPPED INTO 'START TRANSACTION'<>'COMMIT' CLAUSES!!!
	if (!mysql_autocommit(mMysql, 1))
		poco_trace(_logger,"Set autocommit to true");
	else
		poco_error(_logger,"Failed to set autocommit to true");
	/*-------------------------------------*/

	// set connection properties to UTF8 to properly handle locales for different
	// server configs - core sends data in UTF8, so MySQL must expect UTF8 too
	if (!mysql_set_character_set(mMysql,"utf8"))
		poco_trace(_logger,Poco::format("Character set changed to %s",std::string(mysql_character_set_name(mMysql))));
	else
		poco_error(_logger,Poco::format("Failed to change charset, remains at %s",std::string(mysql_character_set_name(mMysql))));

	return 0;
}


bool MySQLConnection::_ConnectionLost(unsigned int errNo) const
{
	const unsigned int CR_SERVER_GONE_ERROR = 2006;
	const unsigned int CR_SERVER_LOST = 2013;
	const unsigned int ER_QUERY_INTERRUPTED = 1317;

	if (!errNo)
		errNo = mysql_errno(mMysql);

	if (errNo == CR_SERVER_GONE_ERROR || errNo == CR_SERVER_LOST || errNo == ER_QUERY_INTERRUPTED)
		return true;

	return false;
}

bool MySQLConnection::_StmtFailed( MYSQL_STMT* stmt, bool* connLost ) const
{
	const unsigned int ER_UNKNOWN_STMT_HANDLER = 1243;
	const unsigned int ER_WRONG_ARGUMENTS = 1210;
	const unsigned int CR_NO_PREPARE_STMT = 2030;
	const unsigned int CR_STMT_CLOSED = 2056;
	const unsigned int CR_NEW_STMT_METADATA = 2057;

	if (connLost)
		*connLost = false;

	unsigned int errNo = mysql_stmt_errno(stmt);
	if (_ConnectionLost(errNo))
	{
		if (connLost)
			*connLost = true;

		return true;
	}

	switch(errNo)
	{
	case ER_UNKNOWN_STMT_HANDLER:
	case ER_WRONG_ARGUMENTS:
	case CR_NO_PREPARE_STMT:
	case CR_STMT_CLOSED:
	case CR_NEW_STMT_METADATA:
		return true;
	default:
		return false;
	}
}

MYSQL_STMT* MySQLConnection::MySQLStmtInit()
{
	return mysql_stmt_init(mMysql);
}

int MySQLConnection::MySQLStmtPrepare(MYSQL_STMT* stmt, const std::string& sql)
{
	int returnVal = 0;
	bool stmtFail = false;
	do 
	{
		bool connLost = false;
		returnVal = mysql_stmt_prepare(stmt, sql.c_str(), sql.length());
		stmtFail = (returnVal && _StmtFailed(stmt,&connLost));

		if (connLost)
		{
			returnVal = _Connect(mMysql);
			if (returnVal)
				break;
		}
	} 
	while (stmtFail);

	return returnVal;
}

int MySQLConnection::MySQLStmtExecute(MYSQL_STMT* &stmt, const std::string& sql, MYSQL_BIND* params)
{
	int returnVal = mysql_stmt_execute(stmt);
	while(returnVal)
	{
		bool connLost = false;
		bool stmtFail = _StmtFailed(stmt, &connLost);

		if (!stmtFail)
			break;

		if (connLost)
		{
			returnVal = _Connect(mMysql);
			if (returnVal)
				break;
		}

		//stmt belonged to another connection, recreate
		mysql_stmt_close(stmt);
		stmt = this->MySQLStmtInit();
		if (stmt == NULL)
			return 1;

		//gotta re-prepare
		returnVal = mysql_stmt_prepare(stmt, sql.c_str(), sql.length());
		if (returnVal) //re-prepare failed
			continue;

		//re-bind params
		if (params != NULL)
		{
			returnVal = mysql_stmt_bind_param(stmt,params);
			if (returnVal) //re-bind failed
				continue;
		}

		//re-execute
		returnVal = mysql_stmt_execute(stmt);
	}

	return returnVal;
}

int MySQLConnection::_MySQLQuery(const char* sql)
{
	int returnVal;
	bool connLost;
	do
	{
		returnVal = mysql_query(mMysql,sql);
		connLost = returnVal && _ConnectionLost();

		if (connLost)
		{
			returnVal = _Connect(mMysql);
			if (returnVal)
				break;
		}
	}
	while(connLost);

	return returnVal;
}

bool MySQLConnection::_Query(const char* sql, MYSQL_RES** pResult, MYSQL_FIELD** pFields, UInt64* pRowCount, UInt32* pFieldCount)
{
	if (!mMysql)
		return 0;

	UInt32 _s;
	if (_logger.trace())
		_s = GlobalTimer::getMSTime();

	if(_MySQLQuery(sql))
	{
		_logger.error(Poco::format("SQL |%s| error %s",string(sql),string(mysql_error(mMysql))));
		return false;
	}
	else if (_logger.trace())
	{
		UInt32 execTime = GlobalTimer::getMSTimeDiff(_s,GlobalTimer::getMSTime());
		_logger.trace(Poco::format("Query [%u ms] SQL: %s",execTime,string(sql)));
	}

	*pResult = mysql_store_result(mMysql);
	*pRowCount = mysql_affected_rows(mMysql);
	*pFieldCount = mysql_field_count(mMysql);

	if (!*pResult )
		return false;

	if (!*pRowCount)
	{
		mysql_free_result(*pResult);
		return false;
	}

	*pFields = mysql_fetch_fields(*pResult);
	return true;
}

QueryResult* MySQLConnection::Query(const char* sql)
{
	MYSQL_RES *result = NULL;
	MYSQL_FIELD *fields = NULL;
	UInt64 rowCount = 0;
	UInt32 fieldCount = 0;

	if(!_Query(sql,&result,&fields,&rowCount,&fieldCount))
		return NULL;

	QueryResultMysql *queryResult = new QueryResultMysql(result, fields, rowCount, fieldCount);

	queryResult->NextRow();
	return queryResult;
}

QueryNamedResult* MySQLConnection::QueryNamed(const char* sql)
{
	MYSQL_RES *result = NULL;
	MYSQL_FIELD *fields = NULL;
	UInt64 rowCount = 0;
	UInt32 fieldCount = 0;

	if(!_Query(sql,&result,&fields,&rowCount,&fieldCount))
		return NULL;

	QueryFieldNames names(fieldCount);
	for (UInt32 i = 0; i < fieldCount; i++)
		names[i] = fields[i].name;

	QueryResultMysql *queryResult = new QueryResultMysql(result, fields, rowCount, fieldCount);

	queryResult->NextRow();
	return new QueryNamedResult(queryResult,names);
}

bool MySQLConnection::Execute(const char* sql)
{
	if (!mMysql)
		return false;

	UInt32 _s;
	if (_logger.trace())
		_s = GlobalTimer::getMSTime();

	if(_MySQLQuery(sql))
	{
		_logger.error(Poco::format("SQL |%s| error %s",string(sql),string(mysql_error(mMysql))));
		return false;
	}
	else if (_logger.trace())
	{
		UInt32 execTime = GlobalTimer::getMSTimeDiff(_s,GlobalTimer::getMSTime());
		_logger.trace(Poco::format("Execute [%u ms] SQL: %s",execTime,string(sql)));
	}

	return true;
}

bool MySQLConnection::_TransactionCmd(const char* sql)
{
	if (_MySQLQuery(sql))
	{
		_logger.error(Poco::format("SQL |%s| error %s",string(sql),string(mysql_error(mMysql))));
		return false;
	}
	else if (_logger.trace())
	{
		_logger.trace(Poco::format("Transaction SQL: %s",string(sql)));
	}
	return true;
}

bool MySQLConnection::BeginTransaction()
{
	return _TransactionCmd("START TRANSACTION");
}

bool MySQLConnection::CommitTransaction()
{
	return _TransactionCmd("COMMIT");
}

bool MySQLConnection::RollbackTransaction()
{
	return _TransactionCmd("ROLLBACK");
}

unsigned long MySQLConnection::escape_string(char* to, const char* from, unsigned long length)
{
	if (!mMysql || !to || !from || !length)
		return 0;

	return(mysql_real_escape_string(mMysql, to, from, length));
}

//////////////////////////////////////////////////////////////////////////
SqlPreparedStatement* MySQLConnection::CreateStatement( const std::string& fmt )
{
	return new MySqlPreparedStatement(fmt, *this);
}

//////////////////////////////////////////////////////////////////////////
MySqlPreparedStatement::MySqlPreparedStatement( const std::string& fmt, SqlConnection& conn ) : SqlPreparedStatement(fmt, conn),
	m_mySqlConn(static_cast<MySQLConnection&>(conn)), m_stmt(NULL), m_pInputArgs(NULL), m_pResult(NULL), m_pResultMetadata(NULL), _logger(conn.DB().CreateLogger("MySqlPreparedStatement")) {}

MySqlPreparedStatement::~MySqlPreparedStatement()
{
	RemoveBinds();
}

bool MySqlPreparedStatement::prepare()
{
	if(isPrepared())
		return true;

	//remove old binds
	RemoveBinds();

	//create statement object
	m_stmt = m_mySqlConn.MySQLStmtInit();
	if (!m_stmt)
	{
		_logger.error("SQL: mysql_stmt_init() failed ");
		return false;
	}

	//prepare statement
	if (m_mySqlConn.MySQLStmtPrepare(m_stmt, m_szFmt))
	{
		_logger.error(Poco::format("SQL: mysql_stmt_prepare() failed for '%s' with ERROR %s",m_szFmt,string(mysql_stmt_error(m_stmt))));
		return false;
	}

	/* Get the parameter count from the statement */
	m_nParams = mysql_stmt_param_count(m_stmt);

	/* Fetch result set meta information */
	m_pResultMetadata = mysql_stmt_result_metadata(m_stmt);
	//if we do not have result metadata
	if (!m_pResultMetadata && m_szFmt.length() >= 6 && (!Poco::icompare(m_szFmt.substr(0,6), "select")))
	{
		_logger.error(Poco::format("SQL: no meta information for '%s', ERROR %s",m_szFmt,string(mysql_stmt_error(m_stmt))));
		return false;
	}

	//bind input buffers
	if(m_nParams)
	{
		if (!m_pInputArgs)
			m_pInputArgs = new MYSQL_BIND[m_nParams];

		memset(m_pInputArgs, 0, sizeof(MYSQL_BIND) * m_nParams);
	}

	//check if we have a statement which returns result sets
	if(m_pResultMetadata)
	{
		//our statement is query
		m_bIsQuery = true;
		/* Get total columns in the query */
		m_nColumns = mysql_num_fields(m_pResultMetadata);

		//bind output buffers
	}

	m_bPrepared = true;
	return true;
}

void MySqlPreparedStatement::bind( const SqlStmtParameters& holder )
{
	if(!isPrepared())
	{
		poco_bugcheck();
		return;
	}

	//finalize adding params
	if(!m_pInputArgs)
		return;

	//verify if we bound all needed input parameters
	if(m_nParams != holder.boundParams())
	{
		poco_bugcheck();
		return;
	}

	int nIndex = 0;
	SqlStmtParameters::ParameterContainer const& _args = holder.params();

	for (SqlStmtParameters::ParameterContainer::const_iterator iter = _args.begin(); iter != _args.end(); ++iter)
	{
		//bind parameter
		addParam(nIndex++, (*iter));
	}

	//bind input arguments
	if(mysql_stmt_bind_param(m_stmt, m_pInputArgs))
	{
		_logger.error(Poco::format("SQL ERROR: mysql_stmt_bind_param() failed with ERROR %s",string(mysql_stmt_error(m_stmt))));
	}
}

void MySqlPreparedStatement::addParam( int nIndex, const SqlStmtFieldData& data )
{
	poco_assert(m_pInputArgs);
	poco_assert(nIndex < m_nParams);

	MYSQL_BIND& pData = m_pInputArgs[nIndex];

	my_bool bUnsigned = 0;
	enum_field_types dataType = ToMySQLType(data, bUnsigned);

	//setup MYSQL_BIND structure
	pData.buffer_type = dataType;
	pData.is_unsigned = bUnsigned;
	pData.buffer = data.buff();
	pData.length = 0;
	pData.buffer_length = (data.type() == FIELD_STRING || data.type() == FIELD_BINARY) ? data.size() : 0;
}

void MySqlPreparedStatement::RemoveBinds()
{
	if(!m_stmt)
		return;

	delete [] m_pInputArgs;
	m_pInputArgs = NULL;
	delete [] m_pResult;
	m_pResult = NULL;

	mysql_free_result(m_pResultMetadata);
	mysql_stmt_close(m_stmt);

	m_stmt = NULL;
	m_pResultMetadata = NULL;

	m_bPrepared = false;
}

std::string MySqlPreparedStatement::_BindParamsToStr() const
{
	string values;
	if (m_pInputArgs != NULL && m_nParams > 0)
	{
		std::ostringstream str;
		str << " " << "VALUES(";
		for(UInt32 i=0;i<m_nParams;i++)
		{
			str << MySQLParamToString(m_pInputArgs[i]);
			if (i != m_nParams-1)
				str << ", ";
		}
		str << ")";
		values = str.str();
	}
	return values;
}

bool MySqlPreparedStatement::execute()
{
	if(!isPrepared())
		return false;

	UInt32 _s;
	if (_logger.trace())
		_s = GlobalTimer::getMSTime();

	if(m_mySqlConn.MySQLStmtExecute(m_stmt,m_szFmt,m_pInputArgs))
	{
		_logger.error(Poco::format("SQL: cannot execute '%s' %s, ERROR %s",m_szFmt,_BindParamsToStr(),string(mysql_stmt_error(m_stmt))));
		return false;
	}
	else if (_logger.trace())
	{
		UInt32 execTime = GlobalTimer::getMSTimeDiff(_s,GlobalTimer::getMSTime());
		_logger.trace(Poco::format("Execute [%u ms] Statement: %s%s",execTime,m_szFmt,_BindParamsToStr()));
	}

	return true;
}

enum_field_types MySqlPreparedStatement::ToMySQLType( const SqlStmtFieldData &data, my_bool &bUnsigned )
{
	bUnsigned = 0;
	enum_field_types dataType = MYSQL_TYPE_NULL;

	switch (data.type())
	{
	case FIELD_NONE:    dataType = MYSQL_TYPE_NULL;                     break;
		// MySQL does not support MYSQL_TYPE_BIT as input type
	case FIELD_BOOL:    //dataType = MYSQL_TYPE_BIT;      bUnsigned = 1;  break;
	case FIELD_UI8:     dataType = MYSQL_TYPE_TINY;     bUnsigned = 1;  break;
	case FIELD_I8:      dataType = MYSQL_TYPE_TINY;                     break;
	case FIELD_I16:     dataType = MYSQL_TYPE_SHORT;                    break;
	case FIELD_UI16:    dataType = MYSQL_TYPE_SHORT;    bUnsigned = 1;  break;
	case FIELD_I32:     dataType = MYSQL_TYPE_LONG;                     break;
	case FIELD_UI32:    dataType = MYSQL_TYPE_LONG;     bUnsigned = 1;  break;
	case FIELD_I64:     dataType = MYSQL_TYPE_LONGLONG;                 break;
	case FIELD_UI64:    dataType = MYSQL_TYPE_LONGLONG; bUnsigned = 1;  break;
	case FIELD_FLOAT:   dataType = MYSQL_TYPE_FLOAT;                    break;
	case FIELD_DOUBLE:  dataType = MYSQL_TYPE_DOUBLE;                   break;
	case FIELD_STRING:  dataType = MYSQL_TYPE_STRING;                   break;
	case FIELD_BINARY:	dataType = MYSQL_TYPE_BLOB; bUnsigned = 1;		break;
	}

	return dataType;
}

#include <boost/lexical_cast.hpp>
#include <Poco/HexBinaryEncoder.h>

string MySqlPreparedStatement::MySQLParamToString( const MYSQL_BIND& par )
{
	using boost::lexical_cast;

	switch (par.buffer_type)
	{
	case MYSQL_TYPE_TINY:
		if (par.is_unsigned)
		{
			UInt8 dest;
			memcpy(&dest,par.buffer,sizeof(dest));
			return lexical_cast<string>(static_cast<UInt32>(dest));
		}
		else
		{
			Int8 dest;
			memcpy(&dest,par.buffer,sizeof(dest));
			return lexical_cast<string>(static_cast<UInt32>(dest));
		}
	case MYSQL_TYPE_SHORT:
		if (par.is_unsigned)
		{
			UInt16 dest;
			memcpy(&dest,par.buffer,sizeof(dest));
			return lexical_cast<string>(dest);
		}
		else
		{
			Int16 dest;
			memcpy(&dest,par.buffer,sizeof(dest));
			return lexical_cast<string>(dest);
		}
	case MYSQL_TYPE_LONG:
		if (par.is_unsigned)
		{
			UInt32 dest;
			memcpy(&dest,par.buffer,sizeof(dest));
			return lexical_cast<string>(dest);
		}
		else
		{
			Int32 dest;
			memcpy(&dest,par.buffer,sizeof(dest));
			return lexical_cast<string>(dest);
		}
	case MYSQL_TYPE_LONGLONG:
		if (par.is_unsigned)
		{
			UInt64 dest;
			memcpy(&dest,par.buffer,sizeof(dest));
			return lexical_cast<string>(dest);
		}
		else
		{
			Int64 dest;
			memcpy(&dest,par.buffer,sizeof(dest));
			return lexical_cast<string>(dest);
		}
	case MYSQL_TYPE_FLOAT:
		{
			float dest;
			memcpy(&dest,par.buffer,sizeof(dest));
			return lexical_cast<string>(dest);
		}
	case MYSQL_TYPE_DOUBLE:
		{
			double dest;
			memcpy(&dest,par.buffer,sizeof(dest));
			return lexical_cast<string>(dest);
		}
	case MYSQL_TYPE_STRING:
		return "\"" + string((const char*)par.buffer,par.buffer_length) + "\"";
	case MYSQL_TYPE_BLOB:
		{
			std::ostringstream ss;
			Poco::HexBinaryEncoder(ss).write((const char*)par.buffer,par.buffer_length);
			ss.flush();
			return "HEX(" + ss.str() + ")";
		}
	default:
		return string();
	}
}

#endif
