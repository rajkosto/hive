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

#include "HiveConsoleChannel.h"
#include <Poco/Message.h>
#include <Poco/UnicodeConverter.h>

#include <fcntl.h>
#include <io.h>

namespace
{
	void RedirectIOToConsole()
	{
		//allocate a console for this app
		if (!AllocConsole())
			return;

		//set the screen buffer to be big enough to let us scroll text
		{
			//maximum mumber of lines the output console should have
			static const WORD MAX_CONSOLE_LINES = 500;

			CONSOLE_SCREEN_BUFFER_INFO coninfo;
			GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&coninfo);
			coninfo.dwSize.Y = MAX_CONSOLE_LINES;
			SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE),coninfo.dwSize);
		}		

		//redirect unbuffered STDOUT to the console
		{
			intptr_t stdHandle = (intptr_t)GetStdHandle(STD_OUTPUT_HANDLE);
			int fHandle = _open_osfhandle(stdHandle,_O_TEXT);
			FILE* fp = _fdopen(fHandle,"w");
			*stdout = *fp;
			setvbuf(stdout,nullptr,_IONBF,0);
		}	

		//redirect unbuffered STDIN to the console
		{
			intptr_t stdHandle = (intptr_t)GetStdHandle(STD_INPUT_HANDLE);
			int fHandle = _open_osfhandle(stdHandle,_O_TEXT);
			FILE* fp = _fdopen(fHandle,"r");
			*stdin = *fp;
			setvbuf(stdin,nullptr,_IONBF,0);
		}

		//redirect unbuffered STDERR to the console
		{
			intptr_t stdHandle = (intptr_t)GetStdHandle(STD_ERROR_HANDLE);
			int fHandle = _open_osfhandle(stdHandle,_O_TEXT);
			FILE* fp = _fdopen(fHandle,"w");
			*stderr = *fp;
			setvbuf(stderr,nullptr,_IONBF,0);
		}

		//make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog point to console as well
		std::ios::sync_with_stdio();
	}
};

HiveConsoleChannel::HiveConsoleChannel(std::string windowTitle)
{
	RedirectIOToConsole();
	if (windowTitle.length() > 0)
	{
		std::wstring wideTitle;
		Poco::UnicodeConverter::toUTF16(windowTitle,wideTitle);
		SetConsoleTitleW(wideTitle.c_str());
	}

	_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
}

HiveConsoleChannel::~HiveConsoleChannel()
{
}

void HiveConsoleChannel::log(const Poco::Message& msg)
{
	if (!shouldLog(msg.getPriority()))
		return;

	std::string text = msg.getText() + "\r\n";
	{
		std::wstring utext;
		Poco::UnicodeConverter::toUTF16(text,utext);
		DWORD written;
		WriteConsoleW(_hConsole,utext.data(),static_cast<DWORD>(utext.size()),&written,nullptr);
	}	
}