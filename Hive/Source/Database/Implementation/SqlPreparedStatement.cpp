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

#include "SqlPreparedStatement.h"
#include "Database/Database.h"
#include "SqlConnection.h"

#include <Poco/String.h>

SqlPreparedStatement::SqlPreparedStatement( const char* sqlText, SqlConnection& conn ) : 
	_numParams(0), _numColumns(0), _isQuery(false), _prepared(false), 
	_stmtSql(sqlText), _stmtLen(strlen(sqlText)), _conn(conn) { }

//////////////////////////////////////////////////////////////////////////
SqlPlainPreparedStatement::SqlPlainPreparedStatement( const char* sqlText, SqlConnection& conn ) : 
	SqlPreparedStatement(sqlText, conn) {}

void SqlPlainPreparedStatement::prepare()
{
	_numParams = std::count(_stmtSql, _stmtSql+_stmtLen, '?');
	_isQuery = !strnicmp("select",_stmtSql,6);
	_prepared = true;
}

void SqlPlainPreparedStatement::bind( const SqlStmtParameters& holder )
{
	poco_assert(isPrepared());

	//verify if we bound all needed input parameters
	if (_numParams != holder.boundParams())
	{
		poco_bugcheck_msg("Not all parameters bound in SqlPlainPreparedStatement");
		return;
	}

	//reset resulting plain SQL request
	_preparedSql = _stmtSql;
	size_t nLastPos = 0;

	const SqlStmtParameters::ParameterContainer& holderArgs = holder.params();
	for (auto it=holderArgs.cbegin(); it!=holderArgs.cend(); ++it)
	{
		const SqlStmtField& data = (*it);

		nLastPos = _preparedSql.find('?', nLastPos);
		if(nLastPos != std::string::npos)
		{
			//bind parameter
			std::ostringstream fmt;
			dataToString(data, fmt);

			std::string tmp = fmt.str();
			_preparedSql.replace(nLastPos, 1, tmp);
			nLastPos += tmp.length();
		}
	}
}

bool SqlPlainPreparedStatement::execute()
{
	poco_assert(isPrepared());

	if (_preparedSql.empty())
		return false;

	return _conn.execute(_preparedSql.c_str());
}

std::string SqlPlainPreparedStatement::getSqlString( bool withValues/*=false*/ ) const 
{
	if (withValues)
		return _preparedSql;

	return SqlPlainPreparedStatement::getSqlString(withValues);
}

#include <Poco/HexBinaryEncoder.h>
#include "ConcreteDatabase.h"

void SqlPlainPreparedStatement::dataToString( const SqlStmtField& data, std::ostringstream& fmt ) const
{
	switch (data.type())
	{
	case SqlStmtField::FIELD_BOOL:    fmt << "'" << UInt32(data.toBool()) << "'";     break;
		case SqlStmtField::FIELD_UI8:     fmt << "'" << UInt32(data.toUint8()) << "'";    break;
		case SqlStmtField::FIELD_UI16:    fmt << "'" << UInt32(data.toUint16()) << "'";   break;
		case SqlStmtField::FIELD_UI32:    fmt << "'" << data.toUint32() << "'";           break;
		case SqlStmtField::FIELD_UI64:    fmt << "'" << data.toUint64() << "'";           break;
		case SqlStmtField::FIELD_I8:      fmt << "'" << Int32(data.toInt8()) << "'";      break;
		case SqlStmtField::FIELD_I16:     fmt << "'" << Int32(data.toInt16()) << "'";     break;
		case SqlStmtField::FIELD_I32:     fmt << "'" << data.toInt32() << "'";            break;
		case SqlStmtField::FIELD_I64:     fmt << "'" << data.toInt64() << "'";            break;
		case SqlStmtField::FIELD_FLOAT:   fmt << "'" << data.toFloat() << "'";            break;
		case SqlStmtField::FIELD_DOUBLE:  fmt << "'" << data.toDouble() << "'";           break;
		case SqlStmtField::FIELD_STRING:
		{
			std::string tmp = _conn.getDB().escape(data.toString());
			fmt << "'" << tmp << "'";
		}
		break;
		case SqlStmtField::FIELD_BINARY:
		{
			std::ostringstream ss;
			Poco::HexBinaryEncoder(ss).write((char*)data.buff(),data.size());
			ss.flush();
			fmt << "UNHEX('" << ss.str() << "')";
		}
		break;
	}
}