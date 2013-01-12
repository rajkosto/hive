/*
* Copyright (C) 2009-2013 Rajko Stojadinovic <http://github.com/rajkosto/hive>
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

#include "../QueryResultImpl.h"
#include "DatabasePostgre.h"

class QueryResultPostgre : public QueryResultImpl
{
public:
	QueryResultPostgre(PostgreSQLConnection* theConn, const char* sql);
	~QueryResultPostgre();

	bool fetchRow() override;
	QueryFieldNames fetchFieldNames() const override;

	bool nextResult() override;
private:
	vector<PostgreSQLConnection::ResultInfo> _results;
	int _currRes;
	size_t _tblIdx;
};
