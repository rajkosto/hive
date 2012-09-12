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

//MySQL prepared statement class
class MySqlPreparedStatement : public SqlPreparedStatement
{
public:
	MySqlPreparedStatement(const std::string& fmt, SqlConnection& conn);
	~MySqlPreparedStatement();

	//prepare statement
	virtual bool prepare();

	//bind input parameters
	virtual void bind(const SqlStmtParameters& holder);

	//execute DML statement
	virtual bool execute();

protected:
	//bind parameters
	void addParam(int nIndex, const SqlStmtFieldData& data);

	static enum_field_types ToMySQLType( const SqlStmtFieldData& data, my_bool& bUnsigned );
	static string MySQLParamToString( const MYSQL_BIND& par );
private:
	void RemoveBinds();
	std::string _BindParamsToStr() const;

	class MySQLConnection& m_mySqlConn;
	MYSQL_STMT* m_stmt;
	MYSQL_BIND* m_pInputArgs;
	MYSQL_BIND* m_pResult;
	MYSQL_RES* m_pResultMetadata;

	class Poco::Logger& _logger;
};

class MySQLConnection : public SqlConnection
{
public:
	MySQLConnection(Database& db);
	~MySQLConnection();

	//! Initializes Mysql and connects to a server.
	/*! infoString should be formated like hostname;username;password;database. */
	bool Initialize(const std::string& infoString) override;

	QueryResult* Query(const char* sql);
	QueryNamedResult* QueryNamed(const char* sql);
	bool Execute(const char* sql);

	unsigned long escape_string(char* to, const char* from, unsigned long length);

	bool BeginTransaction();
	bool CommitTransaction();
	bool RollbackTransaction();

	MYSQL_STMT* MySQLStmtInit();
	int MySQLStmtPrepare(MYSQL_STMT* stmt, const std::string& sql);
	int MySQLStmtExecute(MYSQL_STMT* &stmt, const std::string& sql, MYSQL_BIND* params);

protected:
	SqlPreparedStatement* CreateStatement(const std::string& fmt);

private:
	bool _TransactionCmd(const char* sql);
	bool _Query(const char* sql, MYSQL_RES** pResult, MYSQL_FIELD** pFields, UInt64* pRowCount, UInt32* pFieldCount);
	int _Connect(MYSQL* mysqlInit);
	bool _ConnectionLost(unsigned int errNo = 0) const;
	bool _StmtFailed(MYSQL_STMT* stmt, bool* connLost = NULL) const;
	int _MySQLQuery(const char* sql);

	std::string _host, _user, _password, _database;
	int _port;
	std::string _unix_socket;

	MYSQL* mMysql;
	class Poco::Logger& _logger;
};

class DatabaseMysql : public ConcreteDatabase
{
public:
	DatabaseMysql();
	virtual ~DatabaseMysql();

	// must be call before first query in thread
	void ThreadStart();
	// must be call before the thread has finished running
	void ThreadEnd();

	//Query creation helpers
	std::string like() const override;
	std::string table_sim(const std::string& tableName) const override;
	std::string concat(const std::string& a, const std::string& b, const std::string& c) const override;
	std::string offset() const override;

protected:
	virtual SqlDelayThread* CreateDelayThread() override;
	virtual SqlConnection* CreateConnection();

private:
	static size_t db_count;
};

#endif