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

#include "Shared/Common/Types.h"
#include <boost/function.hpp>

class QueryResult;
struct QueryCallback
{
	typedef unique_ptr<QueryResult>& ResType; 
	typedef boost::function<void(ResType)> FuncType;

	QueryCallback() : res(nullptr) {}
	QueryCallback(FuncType fun, QueryResult* res = nullptr) : fun(fun), res(res) {}

	void invoke() 
	{ 
		unique_ptr<QueryResult> pRes(res);
		res = nullptr;

		//transfer ownership to callback function so that it can stash it or use it later
		if (!fun.empty()) 
			fun(pRes);
	}
	void setResult(QueryResult* res) { this->res = res; }
protected:
	FuncType fun;
	QueryResult* res;
};