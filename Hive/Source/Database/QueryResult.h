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
#include "Shared/Common/Exception.h"
#include <boost/noncopyable.hpp>

#include "Field.h"

class QueryResult : public boost::noncopyable
{
public:
	virtual ~QueryResult() {}

	//call before accessing, to populate the row.
	//false return value means that there's no more rows
	virtual bool fetchRow() = 0;

	virtual const vector<Field>& fields() const = 0;

	const Field& at(size_t idx) const
	{
		if (idx < fields().size())
			return fields()[idx];
		else
			return _dummyField;
	}
	const Field& operator [] (size_t idx) const { return at(idx); }

	virtual size_t numFields() const = 0;
	//this will also return number of affected rows for non-SELECT statements
	virtual UInt64 numRows() const = 0;

protected:
	Field _dummyField;
};

typedef std::vector<std::string> QueryFieldNames;

class QueryNamedResult : public QueryResult
{
public:
	QueryNamedResult(unique_ptr<QueryResult> query, const QueryFieldNames& names) : _actualRes(std::move(query)), _fieldNames(names) {}
	~QueryNamedResult() {}

	bool fetchRow() override { return _actualRes->fetchRow(); }
	const vector<Field>& fields() const override { return _actualRes->fields(); }

	size_t numFields() const override { return _actualRes->numFields(); }
	UInt64 numRows() const override { return _actualRes->numRows(); }

	//named access
	const Field& operator[] (const std::string& name) const { return (*_actualRes)[fieldIdx(name)]; }
	const QueryFieldNames& fieldNames() const { return _fieldNames; }

	size_t fieldIdx(const std::string& name) const
	{
		for(size_t idx=0; idx<_fieldNames.size(); idx++)
		{
			if(_fieldNames[idx] == name)
				return idx;
		}
		poco_bugcheck_msg("unknown field name");
		return size_t(-1);
	}

protected:
	unique_ptr<QueryResult> _actualRes;
	QueryFieldNames _fieldNames;
};