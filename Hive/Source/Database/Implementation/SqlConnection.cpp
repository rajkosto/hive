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

#include "SqlConnection.h"
#include "Database/Database.h"
#include "SqlPreparedStatement.h"

//////////////////////////////////////////////////////////////////////////
SqlPreparedStatement* SqlConnection::CreateStatement( const std::string& fmt )
{
	return new SqlPlainPreparedStatement(fmt, *this);
}

void SqlConnection::clear()
{
	SqlConnection::Lock guard(this);
	m_stmtHolder.clear();
}

SqlPreparedStatement* SqlConnection::GetStmt( const SqlStatementID& stId )
{
	if(!stId.isInitialized())
		return NULL;

	UInt32 stmtId = stId.getId();
	SqlPreparedStatement* pStmt = m_stmtHolder.getPrepStmtObj(stmtId);

	//create stmt obj if needed
	if(pStmt == NULL)
	{
		//obtain SQL request string
		std::string fmt = m_db.GetStmtString(stmtId);
		poco_assert(fmt.length());
		//allocate SQlPreparedStatement object
		pStmt = CreateStatement(fmt);
		//prepare statement
		if(!pStmt->prepare())
		{
			poco_bugcheck_msg("Unable to prepare SQL statement");
			return NULL;
		}

		//save statement in internal registry
		m_stmtHolder.insertPrepStmtObj(stmtId,pStmt);
	}

	return pStmt;
}

bool SqlConnection::ExecuteStmt(const SqlStatementID& stId, const SqlStmtParameters& params )
{
	if(!stId.isInitialized())
		return false;

	//get prepared statement object
	SqlPreparedStatement* pStmt = GetStmt(stId);
	//bind parameters
	pStmt->bind(params);
	//execute statement
	return pStmt->execute();
}

unsigned long SqlConnection::escape_string( char* to, const char* from, unsigned long length )
{
	strncpy(to,from,length); 
	return length;
}

bool SqlConnection::BeginTransaction()
{
	return true;
}

bool SqlConnection::CommitTransaction()
{
	return true;
}

bool SqlConnection::RollbackTransaction()
{
	return true;
}

void SqlConnection::StmtHolder::clear()
{
	for (StatementObjMap::iterator it=_map.begin();it!=_map.end();++it)
		delete it->second;

	_map.clear();
}

SqlPreparedStatement* SqlConnection::StmtHolder::getPrepStmtObj( UInt32 stmtId ) const
{
	StatementObjMap::const_iterator iter = _map.find(stmtId);
	if(iter == _map.end())
		return NULL;
	else
		return iter->second;
}

void SqlConnection::StmtHolder::insertPrepStmtObj( UInt32 stmtId, SqlPreparedStatement* stmtObj )
{
	StatementObjMap::iterator iter = _map.find(stmtId);
	if(iter != _map.end())
		delete iter->second;

	_map[stmtId] = stmtObj;
}
