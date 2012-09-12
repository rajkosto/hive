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
		m_index = index.m_index;
		m_pDB = index.m_pDB;
		m_pParams = NULL;

		if(index.m_pParams)
			m_pParams = new SqlStmtParameters(*(index.m_pParams));
	}
	virtual ~SqlStatementImpl()
	{
	}

	SqlStatementImpl& operator=( const SqlStatementImpl& index )
	{
		if(this != &index)
		{
			m_index = index.m_index;
			m_pDB = index.m_pDB;

			if(m_pParams)
			{
				delete m_pParams;
				m_pParams = NULL;
			}

			if(index.m_pParams)
				m_pParams = new SqlStmtParameters(*(index.m_pParams));
		}

		return *this;
	}
	bool Execute();
	bool DirectExecute();
protected:
	//don't allow anyone except Database class to create static SqlStatement objects
	friend class ConcreteDatabase;
	SqlStatementImpl(const SqlStatementID& index, ConcreteDatabase& db) 
	{
		m_index = index;
		m_pDB = &db;
		m_pParams = NULL;
	}
private:
	SqlStmtParameters* detach()
	{
		SqlStmtParameters* p = m_pParams ? m_pParams : new SqlStmtParameters(0);
		m_pParams = NULL;
		return p;
	}
	void VerifyNumBoundParams( SqlStmtParameters* args );

	ConcreteDatabase* m_pDB;
};