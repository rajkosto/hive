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
#include "Database/SqlStatement.h"

#include <stdexcept>
#include <sstream>

class Database;
class SqlConnection;
class QueryResult;

//base prepared statement class
class SqlPreparedStatement
{
public:
	virtual ~SqlPreparedStatement() {}

	bool isPrepared() const { return m_bPrepared; }
	bool isQuery() const { return m_bIsQuery; }

	UInt32 params() const { return m_nParams; }
	UInt32 columns() const { return isQuery() ? m_nColumns : 0; }

	//initialize internal structures of prepared statement
	//upon success m_bPrepared should be true
	virtual bool prepare() = 0;
	//bind parameters for prepared statement from parameter placeholder
	virtual void bind(const SqlStmtParameters& holder) = 0;

	//execute statement w/o result set
	virtual bool execute() = 0;

protected:
	SqlPreparedStatement(const std::string& fmt, SqlConnection& conn) : m_szFmt(fmt), m_nParams(0), m_nColumns(0), m_bPrepared(false), m_bIsQuery(false), m_pConn(conn) {}

	UInt32 m_nParams;
	UInt32 m_nColumns;
	bool m_bIsQuery;
	bool m_bPrepared;
	std::string m_szFmt;
	SqlConnection& m_pConn;
};

//prepared statements via plain SQL string requests
class SqlPlainPreparedStatement : public SqlPreparedStatement
{
public:
	SqlPlainPreparedStatement(const std::string& fmt, SqlConnection& conn);
	~SqlPlainPreparedStatement() {}

	//this statement is always prepared
	virtual bool prepare() { return true; }

	//we should replace all '?' symbols with substrings with proper format
	virtual void bind(const SqlStmtParameters& holder);

	virtual bool execute();

protected:
	void DataToString(const SqlStmtFieldData& data, std::ostringstream& fmt);

	std::string m_szPlainRequest;
};