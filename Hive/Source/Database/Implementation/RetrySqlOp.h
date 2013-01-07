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

#include "SqlConnection.h"
#include "Shared/Common/Timer.h"
#include <boost/function.hpp>
#include <Poco/Format.h>

namespace Retry
{
	template<typename RetVal>
	struct SqlOp
	{
		typedef boost::function<RetVal(SqlConnection&)> FuncType;
		typedef boost::function<std::string()> SqlStrType;

		SqlOp(Poco::Logger& log_, FuncType runMe_, bool throwExc_ = false) : runMe(runMe_), loggerInst(log_), throwExc(throwExc_) {}
		RetVal operator () (SqlConnection& theConn,const char* logStr="",SqlStrType sqlLogFunc = [](){ return ""; })
		{
			for (;;)
			{
				try 
				{ 
					UInt32 startTime;
					if (loggerInst.trace())
						startTime = GlobalTimer::getMSTime();
					RetVal returnMe = runMe(theConn); 
					if (loggerInst.trace())
					{
						std::string sqlStr = sqlLogFunc();
						if (strlen(logStr) < 1 && sqlStr.length() > 0)
							logStr = "Operation";

						if (strlen(logStr) > 0)
						{
							UInt32 execTime = GlobalTimer::getMSTimeDiff(startTime,GlobalTimer::getMSTime());
							std::string formatStr = string(logStr) + " [%u ms]";
							if (sqlStr.length() > 0)
								formatStr += " SQL: '" + sqlStr + "'";

							loggerInst.trace(Poco::format(formatStr,execTime));
						}
					}
					return returnMe;
				}
				catch(const SqlConnection::SqlException& opExc)
				{
					opExc.toLog(loggerInst);
					if (opExc.isConnLost())
					{
						try { theConn.connect(); }
						catch (const SqlConnection::SqlException& connExc)
						{
							connExc.toLog(loggerInst);
							if (throwExc)
								throw connExc;								
							else
								return 0;
						}
					}
					if (throwExc)
						throw opExc;
					else if (opExc.isRepeatable())
						continue;
					else
						return 0;
				}
			}
		}
	private:
		FuncType runMe;
		Poco::Logger& loggerInst;
		bool throwExc;
	};
};