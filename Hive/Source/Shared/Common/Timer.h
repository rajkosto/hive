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

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

class GlobalTimer
{
public:
	//get current server time
	static UInt64 getMSTime64();
	static UInt32 getMSTime();

	//get time difference between two timestamps
	static inline UInt32 getMSTimeDiff(UInt32 oldMSTime, UInt32 newMSTime)
	{
		if (oldMSTime > newMSTime)
		{
			const UInt32 diff_1 = (UInt32(0xFFFFFFFF) - oldMSTime) + newMSTime;
			const UInt32 diff_2 = oldMSTime - newMSTime;

			return std::min(diff_1, diff_2);
		}

		return newMSTime - oldMSTime;
	}

	//get unix time
	static Int32 getTime();
private:
	GlobalTimer();
	GlobalTimer(const GlobalTimer& );
};