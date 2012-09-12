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

//////////////////////////////////////////////////////////////////////////
SqlPlainPreparedStatement::SqlPlainPreparedStatement( const std::string& fmt, SqlConnection& conn ) : SqlPreparedStatement(fmt, conn)
{
	m_bPrepared = true;
	m_nParams = std::count(begin(m_szFmt), end(m_szFmt), '?');
	m_bIsQuery = (m_szFmt.length() >= 6) && (!Poco::icompare(m_szFmt.substr(0,6), "select"));
}

void SqlPlainPreparedStatement::bind( const SqlStmtParameters& holder )
{
	//verify if we bound all needed input parameters
	if(m_nParams != holder.boundParams())
	{
		poco_bugcheck();
		return;
	}

	//reset resulting plain SQL request
	m_szPlainRequest = m_szFmt;
	size_t nLastPos = 0;

	SqlStmtParameters::ParameterContainer const& _args = holder.params();

	SqlStmtParameters::ParameterContainer::const_iterator iter_last = _args.end();
	for (SqlStmtParameters::ParameterContainer::const_iterator iter = _args.begin(); iter != iter_last; ++iter)
	{
		//bind parameter
		const SqlStmtFieldData& data = (*iter);

		std::ostringstream fmt;
		this->DataToString(data, fmt);

		nLastPos = m_szPlainRequest.find('?', nLastPos);
		if(nLastPos != std::string::npos)
		{
			std::string tmp = fmt.str();
			m_szPlainRequest.replace(nLastPos, 1, tmp);
			nLastPos += tmp.length();
		}
	}
}

bool SqlPlainPreparedStatement::execute()
{
	if(m_szPlainRequest.empty())
		return false;

	return m_pConn.Execute(m_szPlainRequest.c_str());
}

#include <Poco/HexBinaryEncoder.h>

void SqlPlainPreparedStatement::DataToString( const SqlStmtFieldData& data, std::ostringstream& fmt )
{
	switch (data.type())
	{
		case FIELD_BOOL:    fmt << "'" << UInt32(data.toBool()) << "'";     break;
		case FIELD_UI8:     fmt << "'" << UInt32(data.toUint8()) << "'";    break;
		case FIELD_UI16:    fmt << "'" << UInt32(data.toUint16()) << "'";   break;
		case FIELD_UI32:    fmt << "'" << data.toUint32() << "'";           break;
		case FIELD_UI64:    fmt << "'" << data.toUint64() << "'";           break;
		case FIELD_I8:      fmt << "'" << Int32(data.toInt8()) << "'";      break;
		case FIELD_I16:     fmt << "'" << Int32(data.toInt16()) << "'";     break;
		case FIELD_I32:     fmt << "'" << data.toInt32() << "'";            break;
		case FIELD_I64:     fmt << "'" << data.toInt64() << "'";            break;
		case FIELD_FLOAT:   fmt << "'" << data.toFloat() << "'";            break;
		case FIELD_DOUBLE:  fmt << "'" << data.toDouble() << "'";           break;
		case FIELD_STRING:
		{
			std::string tmp = m_pConn.DB().escape_string(data.toString());
			fmt << "'" << tmp << "'";
		}
		case FIELD_BINARY:
		{
			std::ostringstream ss;
			Poco::HexBinaryEncoder(ss).write((char*)data.buff(),data.size());
			ss.flush();
			fmt << "UNHEX('" << ss.str() << "')";
		}
		break;
	}
}
