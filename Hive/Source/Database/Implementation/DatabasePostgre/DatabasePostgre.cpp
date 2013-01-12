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

#include "DatabasePostgre.h"
#include "../SqlOperations.h"
#include "QueryResultPostgre.h"

#include <Poco/Logger.h>
#include <Poco/Format.h>

size_t DatabasePostgre::db_count = 0;

DatabasePostgre::DatabasePostgre()
{
	// before first connection
	if( db_count++ == 0 && !PQisthreadsafe() )
		poco_bugcheck_msg("FATAL ERROR: PostgreSQL libpq isn't thread-safe.");
}

DatabasePostgre::~DatabasePostgre()
{

}

unique_ptr<SqlConnection> DatabasePostgre::createConnection(const KeyValueColl& connParams)
{
	return unique_ptr<SqlConnection>(new PostgreSQLConnection(*this,connParams));
}

std::string DatabasePostgre::sqlLike() const 
{
	return "ILIKE";
}

std::string DatabasePostgre::sqlTableSim( const std::string& tableName ) const 
{
	char tempBuf[256];
	sprintf(tempBuf,"\"%s\"",tableName.c_str());
	return string(tempBuf,2+tableName.length());
}

std::string DatabasePostgre::sqlConcat( const std::string& a, const std::string& b, const std::string& c ) const 
{
	char tempBuf[512];
	sprintf(tempBuf,"( %s || %s || %s )",a.c_str(),b.c_str(),c.c_str());
	return string(tempBuf,12+a.length()+b.length()+c.length());
}

std::string DatabasePostgre::sqlOffset() const 
{
	return "LIMIT 1 OFFSET %d";
}

PostgreSQLConnection::PostgreSQLConnection(ConcreteDatabase& parent, const Database::KeyValueColl& connParams) 
	: SqlConnection(parent), _pgConn(nullptr) 
{
	for (auto it=connParams.cbegin();it!=connParams.cend();++it)
	{
		if (it->first == "host")
			_host = it->second;
		else if (it->first == "port")
			_port = it->second;
		else if (it->first == "username")
			_user = it->second;
		else if (it->first == "password")
			_password = it->second;
		else if (it->first == "database")
			_database = it->second;
	}
}

PostgreSQLConnection::~PostgreSQLConnection()
{
	if (_pgConn != nullptr)
	{
		PQfinish(_pgConn);
		_pgConn = nullptr;
	}
}

bool PostgreSQLConnection::_ConnectionLost() const
{
	if (PQstatus(_pgConn) != CONNECTION_OK)
		return true;
	
	return false;
}

void PostgreSQLConnection::connect()
{
	bool reconnecting = false;
	if (_pgConn != nullptr) //reconnection attempt
	{
		if (!_ConnectionLost())
			return;
		else
			reconnecting = true;
	}

	//remove any state from previous session
	this->clear();

	Poco::Logger& logger = _dbEngine->getLogger();
	for(;;)
	{
		if (reconnecting)
			PQreset(_pgConn);
		else
		{
			if (_host == ".")
				_pgConn = PQsetdbLogin(nullptr, _port == "." ? nullptr : _port.c_str(), nullptr, nullptr, _database.c_str(), _user.c_str(), _password.c_str());
			else
				_pgConn = PQsetdbLogin(_host.c_str(), _port.c_str(), nullptr, nullptr, _database.c_str(), _user.c_str(), _password.c_str());
		}
		
		//check to see that the backend connection was successfully made
		if (_ConnectionLost())
		{
			const char* actionToDo = "connect";
			if (reconnecting)
				actionToDo = "reconnect";

			static const long sleepTime = 1000;
			logger.warning(Poco::format("Could not %s to Postgre database at %s: %s, retrying in %d seconds",
				string(actionToDo),_host,lastErrorDescr(),static_cast<int>(sleepTime/1000)));
			Poco::Thread::sleep(sleepTime);

			continue;
		}
		break;
	}

	string actionDone = (reconnecting)?string("Reconnected"):string("Connected");
	poco_information(logger,Poco::format("%s to Postgre database %s:%s/%s server ver: %d",actionDone,_host,_port,_database,PQserverVersion(_pgConn)));
}

string PostgreSQLConnection::lastErrorDescr(PGresult* maybeRes) const
{
	if (!_pgConn)
		return "";

	const char* errMsg = nullptr;
	if (maybeRes != nullptr)
		errMsg = PQresultErrorMessage(maybeRes);
	else
		errMsg = PQerrorMessage(_pgConn);

	if (!errMsg)
		return "";

	string returnMe = errMsg;
	if (returnMe.length() > 0)
		returnMe = returnMe.substr(0,returnMe.length()-1);

	return returnMe;
}

bool PostgreSQLConnection::_Query(const char* sql)
{
	if (!_pgConn)
		return false;

	int returnVal = PQsendQuery(_pgConn,sql);
	if (!returnVal)
	{
		bool connLost = _ConnectionLost();
		throw SqlException(returnVal,lastErrorDescr(),"Query",connLost,connLost,sql);
	}

	return true;
}

bool PostgreSQLConnection::_PostgreStoreResult( const char* sql, ResultInfo* outResInfo )
{
	PGresult* outResult = PQgetResult(_pgConn);
	if (!outResult)
		return false;

	ExecStatusType resStatus = PQresultStatus(outResult);
	int outRowCount = 0;
	int outFieldCount = 0;
	if (resStatus == PGRES_TUPLES_OK) //has resultset
	{
		outRowCount = PQntuples(outResult);
		outFieldCount = PQnfields(outResult);
	}
	else if (resStatus == PGRES_COMMAND_OK) //rows affected
	{
		const char* numTuples = PQcmdTuples(outResult);
		if (strlen(numTuples) > 0)
			outRowCount = atoi(numTuples);

		//don't need it anymore
		PQclear(outResult);
		outResult = nullptr;
	}
	else //errored
	{
		PQclear(outResult);
		outResult = nullptr;

		bool connLost = _ConnectionLost();
		throw SqlException(resStatus,lastErrorDescr(outResult),"PostgreStoreResult",connLost,connLost,sql);
	}
	
	if (outResInfo != nullptr)
	{
		outResInfo->pgRes = outResult;
		outResInfo->numFields = outFieldCount;
		outResInfo->numRows = outRowCount;
	}
	else if (outResult != nullptr)
	{
		PQclear(outResult);
		outResult = nullptr;
	}

	return true;
}

unique_ptr<QueryResult> PostgreSQLConnection::query(const char* sql)
{
	if(!_Query(sql))
		return nullptr;

	//it will fetch the results in the constructor
	unique_ptr<QueryResult> queryResult(new QueryResultPostgre(this,sql));
	return queryResult;
}

bool PostgreSQLConnection::execute(const char* sql)
{
	bool qryRes = _Query(sql);
	if (!qryRes)
		return false;

	//eat up results if any
	vector<SqlException> excs;
	for (;;)
	{
		//can't skip result fetch, even on error, untill all are eaten
		try
		{
			if (_PostgreStoreResult(sql) == false)
				break;
		}
		catch (const SqlException& e) {	excs.push_back(e);	}
	}
	if (excs.size() > 0)
		throw excs[0];

	return true;
}

bool PostgreSQLConnection::transactionStart()
{
	return execute("START TRANSACTION");
}

bool PostgreSQLConnection::transactionCommit()
{
	return execute("COMMIT");
}

bool PostgreSQLConnection::transactionRollback()
{
	return execute("ROLLBACK");
}

size_t PostgreSQLConnection::escapeString(char* to, const char* from, size_t length) const
{
	if (!_pgConn || !to || !from || !length)
		return 0;

	return PQescapeStringConn(_pgConn,to,from,length,nullptr);
}