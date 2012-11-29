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

#include "Database/QueryResult.h"

class QueryResultImpl : public QueryResult
{
public:
	QueryResultImpl(UInt64 rowCount, size_t fieldCount) : _row(fieldCount), _numFields(fieldCount), _numRows(rowCount) {}
	~QueryResultImpl() {}

	const vector<Field>& fields() const override { return _row;	}

	size_t numFields() const override { return _numFields; }
	UInt64 numRows() const override { return _numRows; }
protected:
	vector<Field> _row;
	size_t _numFields;
	UInt64 _numRows;
};