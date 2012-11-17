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

#include "SqlStatementImpl.h"
#include "ConcreteDatabase.h"

#include <Poco/Format.h>

//////////////////////////////////////////////////////////////////////////

void SqlStatementImpl::VerifyNumBoundParams( SqlStmtParameters* args )
{
	//verify amount of bound parameters
	if(args->boundParams() != this->numArgs())
	{
		string errMsg = Poco::format("SQL ERROR: wrong amount of parameters (%d instead of %d) in statement %s",
			args->boundParams(),this->numArgs(),m_pDB->GetStmtString(this->getId()));
		poco_bugcheck_msg(errMsg.c_str());
	}
}

bool SqlStatementImpl::Execute()
{
	SqlStmtParameters *args = detach();
	VerifyNumBoundParams(args);
	return m_pDB->ExecuteStmt(m_index, args);
}

bool SqlStatementImpl::DirectExecute()
{
	SqlStmtParameters *args = detach();
	VerifyNumBoundParams(args);
	return m_pDB->DirectExecuteStmt(m_index, args);
}