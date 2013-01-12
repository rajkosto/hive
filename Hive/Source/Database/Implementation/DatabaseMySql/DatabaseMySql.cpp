/*
* Copyright (C) 2009-2013 Rajko Stojadinovic <http://github.com/rajkosto/hive>
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

#include "DatabaseMySql.h"
#include "QueryResultMySql.h"

#include <Poco/Logger.h>
#include <Poco/Format.h>

size_t DatabaseMySql::db_count = 0;

void DatabaseMySql::threadEnter()
{
	mysql_thread_init();
}

void DatabaseMySql::threadExit()
{
	mysql_thread_end();
}

DatabaseMySql::DatabaseMySql()
{
	// before first connection
	if (db_count++ == 0)
	{
		// Mysql Library Init
		mysql_library_init(-1, nullptr, nullptr);

		if (!mysql_thread_safe())
			poco_bugcheck_msg("FATAL ERROR: Used MySQL library isn't thread-safe.");
	}
}

DatabaseMySql::~DatabaseMySql()
{
	stopServer();

	//Free Mysql library pointers for last ~DB
	if (--db_count == 0)
		mysql_library_end();
}


unique_ptr<SqlConnection> DatabaseMySql::createConnection(const KeyValueColl& connParams)
{
	return unique_ptr<SqlConnection>(new MySQLConnection(*this,connParams));
}

std::string DatabaseMySql::sqlLike() const 
{
	return "LIKE";
}

std::string DatabaseMySql::sqlTableSim( const std::string& tableName ) const 
{
	char tempBuf[256];
	sprintf(tempBuf,"`%s`",tableName.c_str());
	return string(tempBuf,2+tableName.length());
}

std::string DatabaseMySql::sqlConcat( const std::string& a, const std::string& b, const std::string& c ) const 
{
	char tempBuf[512];
	sprintf(tempBuf,"CONCAT( %s , %s , %s )",a.c_str(),b.c_str(),c.c_str());
	return string(tempBuf,16+a.length()+b.length()+c.length());
}

std::string DatabaseMySql::sqlOffset() const 
{
	return "LIMIT %d,1";
}

MySQLConnection::MySQLConnection( ConcreteDatabase& db, const Database::KeyValueColl& connParams ) 
	: SqlConnection(db), _myConn(nullptr)
{
	_myHandle = mysql_init(nullptr);
	poco_assert(_myHandle != nullptr);
	{
		//Set charset for the connection string
		mysql_options(_myHandle,MYSQL_SET_CHARSET_NAME,"utf8");
		//Disable automatic reconnection, we will do this manually (to re-create broken statements and such)
		my_bool reconnect = 0;
		mysql_options(_myHandle, MYSQL_OPT_RECONNECT, &reconnect);
	}

	_host = "localhost";
	_user = "root";
	std::string portOrSocket;
	for (auto it=connParams.cbegin();it!=connParams.cend();++it)
	{
		if (it->first == "host")
			_host = it->second;
		else if (it->first == "port")
			portOrSocket = it->second;
		else if (it->first == "username")
			_user = it->second;
		else if (it->first == "password")
			_password = it->second;
		else if (it->first == "database")
			_database = it->second;
	}

#ifdef _WIN32
	//Windows named pipe option
	if(_host==".")                                           
	{
		unsigned int opt = MYSQL_PROTOCOL_PIPE;
		mysql_options(_myHandle,MYSQL_OPT_PROTOCOL,(char const*)&opt);
		_port = 0;
	}
	else
		_port = atoi(portOrSocket.c_str());

	_unixSocket.clear();
#else
	//Unix/Linux socket option
	if(_host==".")
	{
		unsigned int opt = MYSQL_PROTOCOL_SOCKET;
		mysql_options(mysqlInit,MYSQL_OPT_PROTOCOL,(char const*)&opt);
		_host = "localhost";
		_port = 0;
		_unixSocket = portOrSocket;
	}
	else
	{
		_port = atoi(portOrSocket.c_str());
		_unixSocket.clear();
	}
#endif
}

MySQLConnection::~MySQLConnection()
{
	//Closing the handle returned by mysql_init closes the connection as well
	if (_myHandle)
		mysql_close(_myHandle);

	_myConn = nullptr;
	_myHandle = nullptr;
}

//Helper error status functions
namespace
{
	bool IsConnectionErrFatal(unsigned int errNo)
	{
		static const unsigned int errs[] = {
			1044, //ER_DBACCESS_DENIED_ERROR
			1045, //ER_ACCESS_DENIED_ERROR
			2001, //CR_SOCKET_CREATE_ERROR
			2004, //CR_IPSOCK_ERROR
			2007, //CR_VERSION_ERROR
			2047, //CR_CONN_UNKNOW_PROTOCOL
		};

		if (std::find(begin(errs),end(errs),errNo) != end(errs))
			return true;

		return false;
	}

	bool IsConnectionLost(unsigned int errNo)
	{
		static const unsigned int errs[] = {
			1317, //ER_QUERY_INTERRUPTED
			2006, //CR_SERVER_GONE_ERROR
			2013, //CR_SERVER_LOST
			2027, //CR_MALFORMED_PACKET
		};

		if (std::find(begin(errs),end(errs),errNo) != end(errs))
			return true;

		return false;
	}

	std::pair<bool,bool> StmtErrorInfo(unsigned int errNo)
	{
		bool connLost = true;
		bool stmtFatal = false;

		if (!IsConnectionLost(errNo))
		{
			connLost = false;

			static const unsigned int warns[] = {
				1243, //ER_UNKNOWN_STMT_HANDLER
				1210, //ER_WRONG_ARGUMENTS
				2030, //CR_NO_PREPARE_STMT
				2056, //CR_STMT_CLOSED
				2057, //CR_NEW_STMT_METADATA
			};

			//any error other than these is fatal
			if (std::find(begin(warns),end(warns),errNo) == end(warns))
				stmtFatal = true;
		}

		return std::make_pair(connLost,stmtFatal);
	}
};

void MySQLConnection::connect() 
{
	bool reconnecting = false;
	unsigned long oldThreadId = 0;
	if (_myConn != nullptr) //reconnection attempt
	{
		oldThreadId = mysql_thread_id(_myConn);

		if (!mysql_ping(_myConn)) //ping ok
			return;
		else
			reconnecting = true;
	}

	//remove any state from previous session
	this->clear();

	Poco::Logger& logger = _dbEngine->getLogger();
	for(;;)
	{
		const char* unix_socket = nullptr;
		if (_unixSocket.length() > 0)
			unix_socket = _unixSocket.c_str();

		const char* password = nullptr;
		if (_password.length() > 0)
			password = _password.c_str();

		_myConn = mysql_real_connect(_myHandle, _host.c_str(), _user.c_str(), password, _database.c_str(), 
			_port, unix_socket, CLIENT_REMEMBER_OPTIONS | CLIENT_MULTI_RESULTS);
		if (!_myConn)
		{
			const char* actionToDo = "connect";
			if (reconnecting)
				actionToDo = "reconnect";

			unsigned int errNo = mysql_errno(_myHandle);
			if (IsConnectionErrFatal(errNo))
				throw SqlException(errNo,mysql_error(_myHandle),actionToDo);

			static const long sleepTime = 1000;
			logger.warning(Poco::format("Could not %s to MySQL database at %s: %s, retrying in %d seconds",
				string(actionToDo),_host,string(mysql_error(_myHandle)),static_cast<int>(sleepTime/1000)));
			Poco::Thread::sleep(sleepTime);

			continue;
		}
		break;
	}

	string actionDone = (reconnecting)?string("Reconnected"):string("Connected");

	poco_information(logger,Poco::format( actionDone + " to MySQL database %s:%d/%s client ver: %s server ver: %s",
		_host, _port,_database,string(mysql_get_client_info()),string(mysql_get_server_info(_myConn)) ));

	//Autocommit should be ON because without it, MySQL would require everything to be wrapped into a transaction
	if (!mysql_autocommit(_myConn, 1))
		poco_trace(logger,"Set autocommit to true");
	else
		poco_error(logger,"Failed to set autocommit to true");

	//set connection properties to UTF8 to properly handle locales for different server configs
	//core sends data in UTF8, so MySQL must expect UTF8 too
	if (!mysql_set_character_set(_myConn,"utf8"))
		poco_trace(logger,Poco::format("Character set changed to %s",string(mysql_character_set_name(_myConn))));
	else
		poco_error(logger,Poco::format("Failed to change charset, remains at %s",string(mysql_character_set_name(_myConn))));

	//rollback any transactions from old connection
	//if server didn't terminate it yet
	if (reconnecting)
		mysql_kill(_myConn, oldThreadId);
}

MYSQL_STMT* MySQLConnection::_MySQLStmtInit()
{
	return mysql_stmt_init(_myConn);
}

void MySQLConnection::_MySQLStmtPrepare(const SqlPreparedStatement& who, MYSQL_STMT* stmt, const char* sqlText, size_t textLen)
{
	int returnVal = mysql_stmt_prepare(stmt, sqlText, textLen);
	if (returnVal)
	{
		auto errInfo = StmtErrorInfo(who.lastError());
		throw SqlException(who.lastError(),who.lastErrorDescr(),"MySQLStmtPrepare",errInfo.first,!errInfo.second,who.getSqlString());
	}
}

void MySQLConnection::_MySQLStmtExecute(const SqlPreparedStatement& who, MYSQL_STMT* stmt)
{
	int returnVal = mysql_stmt_execute(stmt);
	if (returnVal)
	{
		auto errInfo = StmtErrorInfo(who.lastError());		
		throw SqlException(who.lastError(),who.lastErrorDescr(),"MySQLStmtExecute",errInfo.first,!errInfo.second,who.getSqlString(true));
	}
}

bool MySQLConnection::_Query(const char* sql)
{
	if (!_myConn)
		return false;

	int returnVal = mysql_query(_myConn,sql);
	if (returnVal)
	{
		returnVal = mysql_errno(_myConn);
		bool connLost = IsConnectionLost(returnVal);
		throw SqlException(returnVal,mysql_error(_myConn),"MySQLQuery",connLost,connLost,sql);
	}

	return true;
}

bool MySQLConnection::_MySQLStoreResult(const char* sql, ResultInfo* outResInfo)
{
	MYSQL_RES* outResult = mysql_store_result(_myConn);
	UInt64 outRowCount = 0;
	size_t outFieldCount = 0;
	if (outResult)
	{
		outRowCount = mysql_num_rows(outResult);
		outFieldCount = mysql_num_fields(outResult);
	}
	else if (mysql_field_count(_myConn) == 0) //query doesnt return result set
	{
		outFieldCount = mysql_field_count(_myConn);
		outRowCount = mysql_affected_rows(_myConn);
	}
	else //an error occured (no results when there should be)
	{
		int resultRetVal = mysql_errno(_myConn);
		if (resultRetVal)
			throw SqlException(resultRetVal,mysql_error(_myConn),"MySQLStoreResult",IsConnectionLost(resultRetVal),true,sql);
	}

	if (outResInfo != nullptr)
	{
		outResInfo->myRes = outResult;
		outResInfo->numFields = outFieldCount;
		outResInfo->numRows = outRowCount;
	}
	else
	{
		mysql_free_result(outResult);
		outResult = nullptr;
	}


	int moreResults = mysql_next_result(_myConn);
	if (moreResults == 0)
		return true;
	else if (moreResults == -1)
		return false;
	else //an error occured
	{
		int resultRetVal = mysql_errno(_myConn);
		if (resultRetVal)
			throw SqlException(resultRetVal,mysql_error(_myConn),"MySQLNextResult",IsConnectionLost(resultRetVal),true,sql);
		else
		{
			poco_bugcheck_msg("Can't fetch MySQL result when there should be one, and no error happened");
			return false;
		}
	}
}

unique_ptr<QueryResult> MySQLConnection::query(const char* sql)
{
	if(!_Query(sql))
		return nullptr;

	//it will fetch the results in the constructor
	unique_ptr<QueryResult> queryResult(new QueryResultMySql(this,sql));
	return queryResult;
}

bool MySQLConnection::execute(const char* sql)
{
	bool qryRes = _Query(sql);
	if (!qryRes)
		return false;

	//eat up results if any
	while (_MySQLStoreResult(sql) == true) {}

	return true;
}

bool MySQLConnection::transactionStart()
{
	return execute("START TRANSACTION");
}

bool MySQLConnection::transactionCommit()
{
	return execute("COMMIT");
}

bool MySQLConnection::transactionRollback()
{
	return execute("ROLLBACK");
}

size_t MySQLConnection::escapeString(char* to, const char* from, size_t length) const
{
	if (!_myConn || !to || !from || !length)
		return 0;

	return(mysql_real_escape_string(_myConn, to, from, length));
}

//////////////////////////////////////////////////////////////////////////
SqlPreparedStatement* MySQLConnection::createPreparedStatement( const char* sqlText )
{
	return new MySqlPreparedStatement(sqlText, *this);
}

//////////////////////////////////////////////////////////////////////////
MySqlPreparedStatement::MySqlPreparedStatement( const char* sqlText, MySQLConnection& conn ) : SqlPreparedStatement(sqlText, conn), 
	_mySqlConn(conn), _myStmt(nullptr), _myResMeta(nullptr) {}

MySqlPreparedStatement::~MySqlPreparedStatement()
{
	unprepare();
}

int MySqlPreparedStatement::lastError() const
{
	if (_myStmt)
		return mysql_stmt_errno(_myStmt);

	return SqlPreparedStatement::lastError();
}

std::string MySqlPreparedStatement::lastErrorDescr() const
{
	if (_myStmt)
		return mysql_stmt_error(_myStmt);

	return SqlPreparedStatement::lastErrorDescr();
}

void MySqlPreparedStatement::prepare()
{
	if(isPrepared())
		return;

	//remove old binds
	unprepare();

	//create statement object
	_myStmt = _mySqlConn._MySQLStmtInit();
	poco_assert(_myStmt != nullptr);

	//prepare statement
	_mySqlConn._MySQLStmtPrepare(*this, _myStmt, _stmtSql, _stmtLen);

	//Get the parameter count from the statement
	_numParams = mysql_stmt_param_count(_myStmt);

	//Fetch result set meta information
	_myResMeta = mysql_stmt_result_metadata(_myStmt);
	//if we do not have result metadata
	if (!_myResMeta && (!strnicmp("select",_stmtSql,6)))
		poco_bugcheck_msg(Poco::format("SQL: no meta information for '%s', ERROR %s",this->getSqlString(),this->lastErrorDescr()).c_str());

	//set up bind input buffers
	_myArgs.resize(_numParams);
	for (size_t i=0;i<_myArgs.size();i++)
		memset(&_myArgs[i], 0, sizeof(MYSQL_BIND));

	//check if we have a statement which returns result sets
	if(_myResMeta)
	{
		//our statement is a query
		_isQuery = true;
		//get number of columns of the query
		_numColumns = mysql_num_fields(_myResMeta);

		//set up bind output buffers
		_myRes.resize(_numColumns);
		for (size_t i=0;i<_myRes.size();i++)
		{
			MYSQL_BIND& curr = _myRes[i];
			memset(&curr,0,sizeof(MYSQL_BIND));
			//TODO: implement fetching results from statements
		}
	}

	_prepared = true;
}

void MySqlPreparedStatement::bind( const SqlStmtParameters& holder )
{
	poco_assert(isPrepared());
	poco_assert(_myArgs.size() == _numParams);

	//finalize adding params
	if (_myArgs.size() < 1)
		return;

	//verify if we bound all needed input parameters
	if(_numParams != holder.boundParams())
	{
		poco_bugcheck_msg("Not all parameters bound in MySqlPreparedStatement");
		return;
	}

	size_t nIndex = 0;
	const SqlStmtParameters::ParameterContainer& holderArgs = holder.params();
	for (auto it = holderArgs.begin(); it!=holderArgs.end(); ++it)
	{
		//bind parameter
		addParam(nIndex++, (*it));
	}

	//bind input arguments
	if(mysql_stmt_bind_param(_myStmt, &_myArgs[0]))
		poco_bugcheck_msg((string("mysql_stmt_bind_param() failed with ERROR ")+mysql_stmt_error(_myStmt)).c_str());
}

namespace
{
	std::pair<enum_field_types,bool> ToMySQLType( const SqlStmtField& data )
	{
		bool isUnsigned = false;
		enum_field_types dataType = MYSQL_TYPE_NULL;

		switch (data.type())
		{
		//MySQL does not support MYSQL_TYPE_BIT as input type
		case SqlStmtField::FIELD_BOOL:		//dataType = MYSQL_TYPE_BIT;	isUnsigned = true;	break;
		case SqlStmtField::FIELD_UI8:		dataType = MYSQL_TYPE_TINY;		isUnsigned = true;	break;
		case SqlStmtField::FIELD_I8:		dataType = MYSQL_TYPE_TINY;							break;
		case SqlStmtField::FIELD_I16:		dataType = MYSQL_TYPE_SHORT;						break;
		case SqlStmtField::FIELD_UI16:		dataType = MYSQL_TYPE_SHORT;	isUnsigned = true;	break;
		case SqlStmtField::FIELD_I32:		dataType = MYSQL_TYPE_LONG;							break;
		case SqlStmtField::FIELD_UI32:		dataType = MYSQL_TYPE_LONG;		isUnsigned = true;	break;
		case SqlStmtField::FIELD_I64:		dataType = MYSQL_TYPE_LONGLONG;						break;
		case SqlStmtField::FIELD_UI64:		dataType = MYSQL_TYPE_LONGLONG;	isUnsigned = true;	break;
		case SqlStmtField::FIELD_FLOAT:		dataType = MYSQL_TYPE_FLOAT;						break;
		case SqlStmtField::FIELD_DOUBLE:	dataType = MYSQL_TYPE_DOUBLE;						break;
		case SqlStmtField::FIELD_STRING:	dataType = MYSQL_TYPE_STRING;						break;
		case SqlStmtField::FIELD_BINARY:	dataType = MYSQL_TYPE_BLOB;		isUnsigned = true;	break;
		default:							dataType = MYSQL_TYPE_NULL;							break;
		}

		return std::make_pair(dataType,isUnsigned);
	}
};

void MySqlPreparedStatement::addParam( size_t nIndex, const SqlStmtField& data )
{
	poco_assert(_myArgs.size() == _numParams);
	poco_assert(nIndex < _numParams);

	MYSQL_BIND& pData = _myArgs[nIndex];

	//setup MYSQL_BIND structure
	{
		auto typeInfo = ToMySQLType(data);
		pData.buffer_type = typeInfo.first;
		pData.is_unsigned = typeInfo.second;
	}
	pData.buffer = const_cast<void*>(data.buff());
	pData.length = nullptr;
	pData.buffer_length = data.size();
	pData.is_null = nullptr;
	pData.error = nullptr;
}

void MySqlPreparedStatement::unprepare()
{
	_myArgs.clear();
	_myRes.clear();

	if (_myResMeta)
	{
		mysql_free_result(_myResMeta);
		_myResMeta = nullptr;
	}
	if (_myStmt)
	{
		mysql_stmt_close(_myStmt);
		_myStmt = nullptr;
	}	

	_prepared = false;
}

#include <boost/lexical_cast.hpp>
#include <Poco/HexBinaryEncoder.h>

namespace
{
	std::string MySQLParamToString( const MYSQL_BIND& par )
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
			return "<UNDEFINED>";
		}
	}
};


std::string MySqlPreparedStatement::bindParamsToStr() const
{
	string values;
	{
		std::ostringstream str;
		str << " " << "VALUES(";
		for (size_t i=0;i<_myArgs.size();i++)
		{
			str << MySQLParamToString(_myArgs[i]);
			if (i != _myArgs.size()-1)
				str << ", ";
		}
		str << ")";
		values = str.str();
	}
	return values;
}

bool MySqlPreparedStatement::execute()
{
	poco_assert(isPrepared());

	MYSQL_BIND* args = nullptr;
	if (_myArgs.size() > 0)
		args = &_myArgs[0];

	_mySqlConn._MySQLStmtExecute(*this, _myStmt);

	return true;
}
