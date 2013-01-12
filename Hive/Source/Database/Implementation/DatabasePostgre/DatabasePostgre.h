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

#pragma once

#include "../ConcreteDatabase.h"
#include "../SqlConnection.h"
#include <Poco/Logger.h>

#include "postgre.h"

class PostgreSQLConnection : public SqlConnection
{
public:
	//Initialize PostgreSQL library and store credentials
	//connParams should contain host,[port],username,password,database
	PostgreSQLConnection(ConcreteDatabase& db, const Database::KeyValueColl& connParams);
	virtual ~PostgreSQLConnection();

	//Connect or reconnect using stored credentials
	void connect() override;

	unique_ptr<QueryResult> query(const char* sql) override;
	bool execute(const char* sql) override;

	size_t escapeString(char* to, const char* from, size_t length) const override;

	bool transactionStart() override;
	bool transactionCommit() override;
	bool transactionRollback() override;

	struct ResultInfo
	{
		void clear()
		{
			if (pgRes != nullptr)
			{
				PQclear(pgRes);
				pgRes = nullptr;
			}
			numRows = 0;
			numFields = 0;
		}

		ResultInfo() : pgRes(nullptr) { clear(); }
		~ResultInfo() { clear(); }

		ResultInfo(ResultInfo&& rhs) : pgRes(nullptr)
		{
			clear();

			using std::swap;
			swap(this->pgRes,rhs.pgRes);
			swap(this->numFields,rhs.numFields);
			swap(this->numRows,rhs.numRows);
		}

		PGresult* pgRes;
		int numFields;
		int numRows;
	private:
		//only move construction
		ResultInfo(const ResultInfo& rhs);
	};
	//Returns whether or not result fetching was successfull (false means no more results)
	bool _PostgreStoreResult(const char* sql, ResultInfo* outResInfo = nullptr);
private:
	bool _ConnectionLost() const;
	bool _Query(const char* sql);

	std::string lastErrorDescr(PGresult* maybeRes = nullptr) const;

	std::string _host, _port, _user, _password, _database;
	PGconn* _pgConn;
};

class DatabasePostgre : public ConcreteDatabase
{
public:
	DatabasePostgre();
	~DatabasePostgre();

	//Query creation helpers
	std::string sqlLike() const override;
	std::string sqlTableSim(const std::string& tableName) const override;
	std::string sqlConcat(const std::string& a, const std::string& b, const std::string& c) const override;
	std::string sqlOffset() const override;
protected:
	unique_ptr<SqlConnection> createConnection(const KeyValueColl& connParams) override;
private:
	static size_t db_count;
};
