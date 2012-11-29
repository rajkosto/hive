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

#ifdef MYSQL_ENABLED

#include "Shared/Common/Types.h"
#include "QueryResultImpl.h"

#ifdef WIN32
#include <winsock2.h>
#include <mysql/mysql.h>
#else
#include <mysql.h>
#endif

class QueryResultMysql : public QueryResultImpl
{
public:
	QueryResultMysql(MYSQL_RES* result, MYSQL_FIELD* fields, UInt64 rowCount, size_t fieldCount);
	~QueryResultMysql();

	bool fetchRow() override;
private:
	void finish();

	MYSQL_RES* _myRes;
};

#endif