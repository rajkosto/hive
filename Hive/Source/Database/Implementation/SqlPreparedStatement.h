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

class SqlConnection;
class SqlStmtField;
class SqlStmtParameters;

//base prepared statement class
class SqlPreparedStatement
{
public:
	virtual ~SqlPreparedStatement() {}

	bool isPrepared() const { return _prepared; }
	bool isQuery() const { return _isQuery; }

	size_t numParams() const { return _numParams; }
	size_t numColumns() const { return isQuery()?_numColumns:0; }

	//initialize internal structures of prepared statement
	//upon success _prepared should be true
	virtual void prepare() = 0;
	//bind parameters for prepared statement from parameter placeholder
	virtual void bind(const SqlStmtParameters& holder) = 0;

	//execute statement w/o result set
	virtual bool execute() = 0;

	virtual int lastError() const { return 0; }
	virtual std::string lastErrorDescr() const { return ""; }

	virtual std::string getSqlString(bool withValues=false) const 
	{
		if (_stmtLen > 0)
			return std::string(_stmtSql,_stmtLen);

		return "";
	}
protected:
	SqlPreparedStatement(const char* sqlText, SqlConnection& conn);

	size_t _numParams;
	size_t _numColumns;

	bool _isQuery;
	bool _prepared;

	const char* _stmtSql;
	size_t _stmtLen;

	SqlConnection& _conn;
};

//prepared statements via plain SQL string requests
class SqlPlainPreparedStatement : public SqlPreparedStatement
{
public:
	SqlPlainPreparedStatement(const char* sqlText, SqlConnection& conn);
	~SqlPlainPreparedStatement() {}

	//nothing to do, we already have the query string
	void prepare() override;

	//we should replace all '?' symbols with substrings with proper format
	void bind(const SqlStmtParameters& holder) override;

	bool execute() override;

	std::string getSqlString(bool withValues=false) const override;
protected:
	void dataToString(const SqlStmtField& data, std::ostringstream& fmt) const;

	std::string _preparedSql;
};