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

#include "ExtStartup.h"

#include <fcntl.h>
#include <io.h>
#ifdef UNICODE
#include <Poco/UnicodeConverter.h>
#endif

namespace
{
	void RedirectIOToConsole()
	{
		int hConHandle;
		long lStdHandle;
		CONSOLE_SCREEN_BUFFER_INFO coninfo;
		FILE *fp;

		// allocate a console for this app
		AllocConsole();

		// maximum mumber of lines the output console should have
		const WORD MAX_CONSOLE_LINES = 500;

		// set the screen buffer to be big enough to let us scroll text
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),	&coninfo);
		coninfo.dwSize.Y = MAX_CONSOLE_LINES;
		SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE),	coninfo.dwSize);

		// redirect unbuffered STDOUT to the console
		lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
		hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
		fp = _fdopen( hConHandle, "w" );
		*stdout = *fp;
		setvbuf( stdout, NULL, _IONBF, 0 );

		// redirect unbuffered STDIN to the console
		lStdHandle = (long)GetStdHandle(STD_INPUT_HANDLE);
		hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
		fp = _fdopen( hConHandle, "r" );
		*stdin = *fp;
		setvbuf( stdin, NULL, _IONBF, 0 );

		// redirect unbuffered STDERR to the console
		lStdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);
		hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
		fp = _fdopen( hConHandle, "w" );
		*stderr = *fp;
		setvbuf( stderr, NULL, _IONBF, 0 );

		// make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog
		// point to console as well
		std::ios::sync_with_stdio();
	}

#ifdef UNICODE
#define GetCommandLineWut GetCommandLineW
#define CommandLineToArgv CommandLineToArgvW
#else
#define GetCommandLineWut GetCommandLineA
#define CommandLineToArgv CommandLineToArgvA
#endif

	vector<string> GetCmdLineParams(LPTSTR** argv=NULL, int* argc=NULL)
	{
		LPTSTR cmdLine = GetCommandLineWut();
		int numCmdLineArgs = 0;
		LPTSTR* cmdLineArgs = CommandLineToArgv(cmdLine, &numCmdLineArgs);
		if (argv)
			*argv = cmdLineArgs;
		if (argc)
			*argc = numCmdLineArgs;

		vector<string> result;
		result.reserve(numCmdLineArgs);
		for (int i=0;i<numCmdLineArgs;i++)
		{
			string utf8Arg;
#ifdef UNICODE
			Poco::UnicodeConverter::toUTF8(cmdLineArgs[i],utf8Arg);
#else
			utf8Arg = cmdLineArgs[i];
#endif
			result.push_back(utf8Arg);
		}

		if (cmdLineArgs && argv == NULL)
			LocalFree(cmdLineArgs);

		return result;
	}
};

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace
{
	ExtStartup::MakeAppFunction gMakeAppFunc;

	unique_ptr<HiveExtApp> CreateApp()
	{
		LPTSTR* argv;
		int argc;
		auto cmdLine = GetCmdLineParams(&argv,&argc);

		string serverFolder = "@hive";
		for (auto it=cmdLine.begin();it!=cmdLine.end();++it)
		{
			string starter = "-profiles=";
			if (it->length() < starter.length())
				continue;
			string compareMe = it->substr(0,starter.length());
			if (!boost::iequals(compareMe,starter))
				continue;

			string rest = it->substr(compareMe.length());
			boost::trim(rest);

			serverFolder = rest;
		}

		RedirectIOToConsole();
		{
			std::wstring consoleTitle = TEXT("HiveExt Debug Monitor - ");
			consoleTitle += std::wstring(serverFolder.begin(),serverFolder.end());
			SetConsoleTitle(consoleTitle.c_str());
		}

		unique_ptr<HiveExtApp> theApp(gMakeAppFunc(serverFolder));
		{
			int appRes = theApp->run(argc, argv);
			LocalFree(argv);

			if (appRes == Poco::Util::Application::EXIT_IOERR)
				MessageBox(NULL,TEXT("Error connecting to the service"),TEXT("Hive error"),MB_ICONERROR|MB_OK);
			else if (appRes == Poco::Util::Application::EXIT_DATAERR)
				MessageBox(NULL,TEXT("Error loading required resources"),TEXT("Hive error"),MB_ICONERROR|MB_OK);
			else if (appRes != Poco::Util::Application::EXIT_OK)
				MessageBox(NULL,TEXT("Unknown internal error"),TEXT("Hive error"),MB_ICONERROR|MB_OK);

			if (appRes != Poco::Util::Application::EXIT_OK)
				return nullptr;
		}

		return std::move(theApp);
	}
};

namespace
{
	unique_ptr<HiveExtApp> gApp;
};

void ExtStartup::InitModule( MakeAppFunction makeAppFunc )
{
	gMakeAppFunc = std::move(makeAppFunc);
}

void ExtStartup::ProcessShutdown()
{
	gApp.reset();
}

void CALLBACK RVExtension(char *output, int outputSize, const char* function)
{
	if (!gApp)
	{
		gApp = CreateApp();
		if (!gApp) //error during creation
			ExitProcess(1);
	}

	gApp->callExtension(function, output, outputSize);
}