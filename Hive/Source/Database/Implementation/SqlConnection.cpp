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

void SqlConnection::FreePreparedStatements()
{
	SqlConnection::Lock guard(this);

	size_t nStmts = m_holder.size();
	for (size_t i = 0; i < nStmts; ++i)
		delete m_holder[i];

	m_holder.clear();
}

SqlPreparedStatement* SqlConnection::GetStmt( int nIndex )
{
	if(nIndex < 0)
		return NULL;

	//resize stmt container
	if(m_holder.size() <= nIndex)
		m_holder.resize(nIndex + 1, NULL);

	SqlPreparedStatement * pStmt = NULL;

	//create stmt if needed
	if(m_holder[nIndex] == NULL)
	{
		//obtain SQL request string
		std::string fmt = m_db.GetStmtString(nIndex);
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
		m_holder[nIndex] = pStmt;
	}
	else
		pStmt = m_holder[nIndex];

	return pStmt;
}

bool SqlConnection::ExecuteStmt(int nIndex, const SqlStmtParameters& id )
{
	if(nIndex == -1)
		return false;

	//get prepared statement object
	SqlPreparedStatement* pStmt = GetStmt(nIndex);
	//bind parameters
	pStmt->bind(id);
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
