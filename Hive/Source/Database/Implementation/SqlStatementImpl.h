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

#include "Database/SqlStatement.h"

class ConcreteDatabase;

class SqlStatementImpl : public SqlStatement
{
public:
	SqlStatementImpl(const SqlStatementImpl& index)
	{
		_stmtId = index._stmtId;
		_dbEngine = index._dbEngine;

		if(index._params.boundParams() > 0)
			_params = index._params;
		else
			_params.reset(_stmtId.numArgs());
	}
	virtual ~SqlStatementImpl()
	{
	}

	SqlStatementImpl& operator=( const SqlStatementImpl& index )
	{
		if(this != &index)
		{
			_stmtId = index._stmtId;
			_dbEngine = index._dbEngine;

			if(index._params.boundParams() > 0)
				_params = index._params;
			else
				_params.reset(_stmtId.numArgs());
		}

		return *this;
	}
	bool execute();
	bool directExecute();
protected:
	//don't allow anyone except Database class to create static SqlStatement objects
	friend class ConcreteDatabase;
	SqlStatementImpl(const SqlStatementID& index, ConcreteDatabase& db) 
	{
		_stmtId = index;
		_dbEngine = &db;
		_params.reset(_stmtId.numArgs());
	}
private:
	inline SqlStmtParameters detach()
	{
		SqlStmtParameters retVal;
		_params.swap(retVal);
		return retVal;
	}
	void verifyNumBoundParams(const SqlStmtParameters& args);

	ConcreteDatabase* _dbEngine;
};