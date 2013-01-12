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

#include "QueryResultMySql.h"
#include "DatabaseMySql.h"

QueryResultMySql::QueryResultMySql(MySQLConnection* theConn, const char* sql) : _currRes(-1)
{
	bool hasAnotherResult = false;
	MySQLConnection::ResultInfo resInfo;
	do
	{
		hasAnotherResult = theConn->_MySQLStoreResult(sql,&resInfo);
		_results.push_back(std::move(resInfo));
	} 
	while (hasAnotherResult);

	//gotta have at least one result
	poco_assert(nextResult() == true);
}

QueryResultMySql::~QueryResultMySql() {}

bool QueryResultMySql::fetchRow()
{
	//if we're outta results, there's also no more rows
	if (_currRes < 0 || _currRes >= _results.size())
		return false;

	//current result set is in range
	const auto& theRes = _results[_currRes];

	MYSQL_ROW myRow = mysql_fetch_row(theRes.myRes);
	if (!myRow) //no more rows in this result set
		return false;

	//we got a row, point the pointers
	for (size_t i=0; i<_row.size(); i++)
		_row[i].setValue(myRow[i]);

	return true;
}

namespace
{
	Field::DataTypes MySQLTypeToFieldType(enum_field_types mysqlType)
	{
		switch (mysqlType)
		{
		case FIELD_TYPE_TIMESTAMP:
		case FIELD_TYPE_DATE:
		case FIELD_TYPE_TIME:
		case FIELD_TYPE_DATETIME:
		case FIELD_TYPE_YEAR:
		case FIELD_TYPE_STRING:
		case FIELD_TYPE_VAR_STRING:
		case FIELD_TYPE_BLOB:
		case FIELD_TYPE_SET:
		case FIELD_TYPE_NULL:
			return Field::DB_TYPE_STRING;
		case FIELD_TYPE_TINY:
		case FIELD_TYPE_SHORT:
		case FIELD_TYPE_LONG:
		case FIELD_TYPE_INT24:
		case FIELD_TYPE_LONGLONG:
		case FIELD_TYPE_ENUM:
			return Field::DB_TYPE_INTEGER;
		case FIELD_TYPE_DECIMAL:
		case FIELD_TYPE_FLOAT:
		case FIELD_TYPE_DOUBLE:
			return Field::DB_TYPE_FLOAT;
		default:
			return Field::DB_TYPE_UNKNOWN;
		}
	}
}

bool QueryResultMySql::nextResult()
{
	//is the currently selected result in bounds ? if so, free it
	if (_currRes >= 0 && _currRes < _results.size())
		_results[_currRes].clear();
	//select next result
	_currRes++;

	if (_currRes < _results.size())
	{
		const auto& theRes = _results[_currRes];
		setNumFields(theRes.numFields);
		setNumRows(theRes.numRows);

		_row.resize(theRes.numFields);
		if (_row.size() > 0)
		{
			poco_assert(theRes.myRes != nullptr);
			MYSQL_FIELD* fields = mysql_fetch_fields(theRes.myRes);
			for (size_t i=0; i<_row.size(); i++)
			{
				_row[i].setValue(nullptr);
				_row[i].setType(MySQLTypeToFieldType(fields[i].type));
			}
		}

		return true;
	}
	else
	{
		_results.clear();
		_currRes = -1;

		setNumFields(0);
		setNumRows(0);
		_row.clear();

		return false;
	}
}

QueryFieldNames QueryResultMySql::fetchFieldNames() const
{
	//if we got no result, can't fetch field names
	if (_currRes < 0 || _currRes >= _results.size())
		return QueryFieldNames();

	const auto& theRes = _results[_currRes];
	QueryFieldNames fieldNames(theRes.numFields);
	if (fieldNames.size() > 0)
	{
		poco_assert(theRes.myRes != nullptr);
		MYSQL_FIELD* fields = mysql_fetch_fields(theRes.myRes);
		for (size_t i=0; i<fieldNames.size(); i++)
			fieldNames[i] = fields[i].name;
	}

	return std::move(fieldNames);
}