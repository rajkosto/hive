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

#include "HiveLib/ExtStartup.h"
#include "DirectHiveApp.h"

BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		ExtStartup::InitModule([](string profileFolder){ return new DirectHiveApp(profileFolder); });
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		ExtStartup::ProcessShutdown();
		break;
	}
	return TRUE;
}

int main()
{
	Sqf::runTest();

//#define DEBUG_SPLIT_TESTS
#ifdef DEBUG_SPLIT_TESTS
	using boost::lexical_cast;

	DllMain(NULL,DLL_PROCESS_ATTACH,NULL);
	char testOutBuf[4096];
	RVExtension(testOutBuf,sizeof(testOutBuf),"CHILD:302:1337:");
	Sqf::Parameters objStreamStart = boost::get<Sqf::Parameters>(lexical_cast<Sqf::Value>(string(testOutBuf)));
	poco_assert(boost::get<string>(objStreamStart[0]) == "ObjectStreamStart");
	for (size_t i=0; i<lexical_cast<int>(objStreamStart[1]); i++)
	{
		RVExtension(testOutBuf,sizeof(testOutBuf),"CHILD:302:1337:");
		Sqf::Parameters objInfo = boost::get<Sqf::Parameters>(lexical_cast<Sqf::Value>(string(testOutBuf)));
		string objDump = lexical_cast<string>(Sqf::Value(objInfo));
	}
	RVExtension(testOutBuf,sizeof(testOutBuf),"CHILD:201:12662:[]:[]:[]:[false,false,false,false,false,false,true,10130.1,any,[0.837194,0],0,[0,0]]:false:false:0:0:0:0:[]:0:0:Survivor3_DZ:0:");
	RVExtension(testOutBuf,sizeof(testOutBuf),"CHILD:201:5700692:[80,[2588.59,10073.7,0.001]]:");
	RVExtension(testOutBuf,sizeof(testOutBuf),"CHILD:308:1311:Wire_cat1:0:6255222:[329.449,[10554.4,3054.12,0]]:[]:[]:0:1.055e14:");
	RVExtension(testOutBuf,sizeof(testOutBuf),"CHILD:101:23572678:1311:Audris:");

	DllMain(NULL,DLL_PROCESS_DETACH,NULL);
#endif

	return 0;
}