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
#include "Field.h"

class QueryResult
{
public:
	QueryResult(UInt64 rowCount, UInt32 fieldCount) : mFieldCount(fieldCount), mRowCount(rowCount) {}
	virtual ~QueryResult() {}

	virtual bool NextRow() = 0;

	Field *Fetch() const { return mCurrentRow; }

	const Field & operator [] (int index) const { return mCurrentRow[index]; }

	UInt32 GetFieldCount() const { return mFieldCount; }
	UInt64 GetRowCount() const { return mRowCount; }

protected:
	Field* mCurrentRow;
	UInt32 mFieldCount;
	UInt64 mRowCount;
};

typedef std::vector<std::string> QueryFieldNames;

class QueryNamedResult
{
public:
	explicit QueryNamedResult(QueryResult* query, QueryFieldNames const& names) : mQuery(query), mFieldNames(names) {}
	~QueryNamedResult() { delete mQuery; }

	// compatible interface with QueryResult
	bool NextRow() { return mQuery->NextRow(); }
	Field *Fetch() const { return mQuery->Fetch(); }
	UInt32 GetFieldCount() const { return mQuery->GetFieldCount(); }
	UInt64 GetRowCount() const { return mQuery->GetRowCount(); }
	Field const& operator[] (int index) const { return (*mQuery)[index]; }

	// named access
	Field const& operator[] (const std::string& name) const { return mQuery->Fetch()[GetField_idx(name)]; }
	QueryFieldNames const& GetFieldNames() const { return mFieldNames; }

	UInt32 GetField_idx(const std::string &name) const
	{
		for(size_t idx = 0; idx < mFieldNames.size(); ++idx)
		{
			if(mFieldNames[idx] == name)
				return idx;
		}
		poco_bugcheck_msg("unknown field name");
		return UInt32(-1);
	}

protected:
	QueryResult *mQuery;
	QueryFieldNames mFieldNames;
};