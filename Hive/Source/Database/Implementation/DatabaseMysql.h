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

#ifdef MYSQL_ENABLED

#include "ConcreteDatabase.h"
#include "SqlConnection.h"
#include "SqlPreparedStatement.h"

#ifdef WIN32
#include <winsock2.h>
#include <mysql/mysql.h>
#else
#include <mysql.h>
#endif

class MySQLConnection;
//MySQL prepared statement class
class MySqlPreparedStatement : public SqlPreparedStatement
{
public:
	MySqlPreparedStatement(const char* sqlText, MySQLConnection& conn);
	~MySqlPreparedStatement();

	//prepare statement
	void prepare() override;

	//bind input parameters
	void bind(const SqlStmtParameters& holder) override;

	//execute DML statement
	bool execute() override;

	int lastError() const override;
	std::string lastErrorDescr() const override;

	std::string getSqlString(bool withValues=false) const override
	{
		std::string retStr = SqlPreparedStatement::getSqlString();
		if (withValues)
			retStr += bindParamsToStr();

		return retStr;
	}
protected:
	//bind parameters
	void addParam(size_t nIndex, const SqlStmtField& data);
private:
	void unprepare();
	std::string bindParamsToStr() const;

	class MySQLConnection& _mySqlConn;
	MYSQL_STMT* _myStmt;
	std::vector<MYSQL_BIND>	_myArgs;
	std::vector<MYSQL_BIND>	_myRes;
	MYSQL_RES* _myResMeta;
};

class MySQLConnection : public SqlConnection
{
public:
	//Initialize MySQL library and store credentials
	//infoString should be formated like hostname;username;password;database
	MySQLConnection(ConcreteDatabase& db, const std::string& infoString);
	~MySQLConnection();

	//Connect or reconnect using stored credentials
	void connect() override;

	unique_ptr<QueryResult> query(const char* sql) override;
	unique_ptr<QueryNamedResult> namedQuery(const char* sql) override;
	bool execute(const char* sql);

	size_t escapeString(char* to, const char* from, size_t length) const override;

	bool transactionStart() override;
	bool transactionCommit() override;
	bool transactionRollback() override;

	MYSQL_STMT* _MySQLStmtInit();
	void _MySQLStmtPrepare(const SqlPreparedStatement& who, MYSQL_STMT* stmt, const char* sqlText, size_t textLen);
	void _MySQLStmtExecute(const SqlPreparedStatement& who, MYSQL_STMT* stmt);
protected:
	SqlPreparedStatement* createPreparedStatement(const char* sqlText) override;

private:
	bool _TransactionCmd(const char* sql);
	bool _Query(const char* sql, MYSQL_RES*& outResult, MYSQL_FIELD*& outFields, UInt64& outRowCount, size_t& outFieldCount);
	void _MySQLQuery(const char* sql);
	MYSQL_RES* _MySQLStoreResult(const char* sql, UInt64& outRowCount, size_t& outFieldCount);

	std::string _host, _user, _password, _database;
	int _port;
	std::string _unix_socket;

	MYSQL* _myHandle;
	MYSQL* _myConn;
};

class DatabaseMysql : public ConcreteDatabase
{
public:
	DatabaseMysql();
	~DatabaseMysql();

	// must be call before first query in thread
	void threadEnter() override;
	// must be call before the thread has finished running
	void threadExit() override;

	//Query creation helpers
	std::string sqlLike() const override;
	std::string sqlTableSim(const std::string& tableName) const override;
	std::string sqlConcat(const std::string& a, const std::string& b, const std::string& c) const override;
	std::string sqlOffset() const override;

protected:
	unique_ptr<SqlConnection> createConnection(const std::string& infoString) override;

private:
	static size_t db_count;
};

#endif