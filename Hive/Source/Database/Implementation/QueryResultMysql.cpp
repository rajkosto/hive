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

#include "QueryResultMysql.h"

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

QueryResultMysql::QueryResultMysql(MYSQL_RES* result, MYSQL_FIELD* fields, UInt64 rowCount, size_t fieldCount) :
    QueryResultImpl(rowCount, fieldCount), _myRes(result)
{
	if (fields != nullptr)
	{
		for (size_t i=0; i<numFields(); i++)
			_row[i].setType(MySQLTypeToFieldType(fields[i].type));
	}
}

QueryResultMysql::~QueryResultMysql()
{
    finish();
}

bool QueryResultMysql::fetchRow()
{
    MYSQL_ROW myRow;

    if (!_myRes)
        return false;

    myRow = mysql_fetch_row(_myRes);
    if (!myRow)
    {
        finish();
        return false;
    }

    for (size_t i=0; i<numFields(); i++)
        _row[i].setValue(myRow[i]);

    return true;
}

void QueryResultMysql::finish()
{
	_row.clear();

    if (_myRes)
    {
        mysql_free_result(_myRes);
        _myRes = nullptr;
    }
}

#endif
