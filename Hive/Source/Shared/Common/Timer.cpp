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

#include "Timer.h"
#include <Poco/Timestamp.h>

using Poco::Timestamp;
namespace
{
	Timestamp globalProgramStartTime = Timestamp();
}

UInt64 GlobalTimer::getMSTime64()
{
	//get current time
	const Timestamp currTime = Timestamp();
	//calculate time diff between two world ticks
	//special case: curr_time < old_time - we suppose that our time has not ticked at all
	//this should be constant value otherwise it is possible that our time can start ticking backwards until next world tick!!!
	Timestamp::TimeVal elapsed = globalProgramStartTime.elapsed();
	//possible that globalProgramStartTime not initialized yet, return 0 in that case
	if (elapsed < 0)
		elapsed = 0;
	else //convert to milliseconds
		elapsed /= 1000;

	UInt64 diff = static_cast<UInt64>(elapsed); //always positive now
	return diff;
}

UInt32 GlobalTimer::getMSTime()
{
	UInt64 diff = getMSTime64();
	//clamp it
	UInt32 iRes = UInt32(diff % UInt64(0x00000000FFFFFFFF));
	return iRes;
}

Int32 GlobalTimer::getTime()
{
	return static_cast<Int32>(Timestamp().epochTime());
}
