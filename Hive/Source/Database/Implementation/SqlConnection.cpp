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
#include "ConcreteDatabase.h"
#include "SqlPreparedStatement.h"

#include <sstream>

void SqlConnection::SqlException::toStream( std::ostream& ostr ) const
{
	if (isConnLost())
		ostr << "Connection lost (" << getDescr() << ") during ";
	else
		ostr << "Error " << getCode() << " (" << getDescr() << ") in ";

	ostr << getFunction();

	if (getQuery().length() > 0)
		ostr << " SQL: '" << getQuery() << "'";
	if (isRepeatable())
		ostr << " , retrying...";
}

std::string SqlConnection::SqlException::toString() const
{
	std::ostringstream str;
	toStream(str);
	return str.str();
}

void SqlConnection::SqlException::toLog( Poco::Logger& logger ) const
{
	if (isRepeatable())
		logger.warning(this->toString());
	else
		logger.error(this->toString());
}

//////////////////////////////////////////////////////////////////////////
SqlPreparedStatement* SqlConnection::createPreparedStatement( const char* sqlText )
{
	return new SqlPlainPreparedStatement(sqlText, *this);
}

void SqlConnection::clear()
{
	SqlConnection::Lock guard(*this);
	_stmtHolder.clear();
}

SqlPreparedStatement* SqlConnection::getStmt( const SqlStatementID& stId )
{
	if(!stId.isInitialized())
		return nullptr;

	UInt32 stmtId = stId.getId();
	SqlPreparedStatement* pStmt = _stmtHolder.getPrepStmtObj(stmtId);

	//create stmt obj if needed
	if(pStmt == nullptr)
	{
		//obtain SQL request string
		const char* sqlText = _dbEngine->getStmtString(stmtId);
		if (!sqlText || !sqlText[0])
			poco_bugcheck_msg("Blank sql statment string!");

		//allocate SqlPreparedStatement object
		pStmt = createPreparedStatement(sqlText);
		//prepare statement
		pStmt->prepare();

		//save statement in internal registry
		_stmtHolder.insertPrepStmtObj(stmtId,pStmt);
	}

	return pStmt;
}

bool SqlConnection::executeStmt( const SqlStatementID& stId, const SqlStmtParameters& params )
{
	if(!stId.isInitialized())
		return false;

	//get prepared statement object
	SqlPreparedStatement* pStmt = getStmt(stId);
	//bind parameters
	pStmt->bind(params);
	//execute statement
	try { return pStmt->execute(); }
	catch(const SqlException& e)
	{
		if (e.isConnLost() || e.isRepeatable())
		{
			//destroy prepared statement since there was an error in its execution
			_stmtHolder.releasePrepStmtObj(stId.getId());
		}

		//retry or log error as usual
		throw e;
	}
}

size_t SqlConnection::escapeString( char* to, const char* from, size_t length ) const
{
	strncpy(to,from,length); 
	return length;
}

bool SqlConnection::transactionStart()
{
	return true;
}

bool SqlConnection::transactionCommit()
{
	return true;
}

bool SqlConnection::transactionRollback()
{
	return true;
}

void SqlConnection::StmtHolder::clear()
{
	_storage.clear();
}

SqlPreparedStatement* SqlConnection::StmtHolder::getPrepStmtObj( UInt32 stmtId ) const
{
	if (stmtId < 1)
		return nullptr;

	size_t idx = stmtId-1;

	if (idx < _storage.size())
		return _storage[idx].get();
	else
		return nullptr;
}

void SqlConnection::StmtHolder::insertPrepStmtObj( UInt32 stmtId, SqlPreparedStatement* stmtObj )
{
	if (stmtId < 1)
		poco_bugcheck_msg("Trying to insert uninitialized stmt into conn");

	size_t idx = stmtId-1;
	if (idx >= _storage.size())
		_storage.resize(idx+1);

	if (_storage[idx])
		poco_bugcheck_msg("Inserting conn stmt into already occupied slot");

	_storage[idx].reset(stmtObj);
}

unique_ptr<SqlPreparedStatement> SqlConnection::StmtHolder::releasePrepStmtObj( UInt32 stmtId )
{
	if (stmtId < 1)
		return nullptr;

	size_t idx = stmtId-1;
	if (idx >= _storage.size())
		return nullptr;

	return std::move(_storage[idx]);
}
