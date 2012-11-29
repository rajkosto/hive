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

#include "ArmaConsoleChannel.h"
#include "Poco/Message.h"

namespace
{
	struct FinderData
	{
		FinderData(DWORD wntProcId) 
			: wantedProcessId(wntProcId), targetConsole(nullptr), targetRichEdit(nullptr) {}

		DWORD wantedProcessId;
		HWND targetConsole;
		HWND targetRichEdit;
	};
	BOOL CALLBACK FindConsoleWndProc(HWND hWnd, LPARAM someParam)
	{
		FinderData* data = reinterpret_cast<FinderData*>(someParam);

		DWORD thisProcId;
		GetWindowThreadProcessId(hWnd, &thisProcId);
		if(thisProcId == data->wantedProcessId)
		{
			char classNameStr[256];
			memset(classNameStr,0,sizeof(classNameStr));
			GetClassNameA(hWnd,classNameStr,sizeof(classNameStr)-1);
			if (!stricmp(classNameStr,"#32770")) //Dialog
			{
				data->targetConsole = hWnd;
				return false; //stops enumeration
			}			
		}

		//continue enumeration
		return true;
	}
	BOOL CALLBACK FindRichEditProc(HWND hWnd, LPARAM someParam)
	{
		FinderData* data = reinterpret_cast<FinderData*>(someParam);

		char classNameStr[256];
		memset(classNameStr,0,sizeof(classNameStr));
		GetClassNameA(hWnd,classNameStr,sizeof(classNameStr)-1);
		if (!stricmp(classNameStr,"RichEdit20A"))
		{
			data->targetRichEdit = hWnd;
			return false; //stops enumeration
		}

		return false;
	}
}

#include <Richedit.h>
#include <CommDlg.h>

ArmaConsoleChannel::ArmaConsoleChannel() : _wndRich(nullptr)
{
	//find the rich edit control window in our process
	FinderData fndData(GetProcessId(GetCurrentProcess()));
	EnumWindows(FindConsoleWndProc,reinterpret_cast<LPARAM>(&fndData));
	if (fndData.targetConsole != nullptr)
	{
		EnumChildWindows(fndData.targetConsole,FindRichEditProc,reinterpret_cast<LPARAM>(&fndData));
		if (fndData.targetRichEdit != nullptr)
			_wndRich = fndData.targetRichEdit;
	}
}

ArmaConsoleChannel::~ArmaConsoleChannel()
{
}

#include <detours.h>

namespace 
{ 
	void WINAPI GreatSleep(DWORD millis); 

	class DetourPatcher
	{
	public:
		DetourPatcher()
		{
			RealSleep = Sleep;
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)RealSleep, GreatSleep);
			DetourTransactionCommit();
		}
		~DetourPatcher()
		{
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourDetach(&(PVOID&)RealSleep, GreatSleep);
			DetourTransactionCommit();
		}

		void doSleep(DWORD millis)
		{
			if (millis == 50)
				MsgWaitForMultipleObjects(0,nullptr,false,millis,QS_ALLINPUT);
			else
				RealSleep(millis);
		}
	private:
		typedef void (WINAPI *SleepFuncType)(DWORD);
		SleepFuncType RealSleep;
	} gPatcher;

	void WINAPI GreatSleep(DWORD millis)
	{
		gPatcher.doSleep(millis);
	}
};

void ArmaConsoleChannel::log(const Poco::Message& msg)
{
	if (_wndRich == nullptr)
		return;

	if (!shouldLog(msg.getPriority()))
		return;

	std::string text = msg.getText() + "\r\n";
	if (text[0]	 == '0') //remove prefix from hour to match arma
		text = " " + text.substr(1);

	static const size_t MAX_LINE_COUNT = 500;
	size_t lineCount = SendMessageW(_wndRich,EM_GETLINECOUNT,0,0);
	int lastPoint = -1;
	while (lineCount > MAX_LINE_COUNT)
	{
		FINDTEXTEXA findInfo;
		//search document from last point
		findInfo.chrg.cpMin = lastPoint+1;
		findInfo.chrg.cpMax = -1;
		//search for newline
		findInfo.lpstrText = "\r";
		//no-find return
		findInfo.chrgText.cpMin = -1;
		findInfo.chrgText.cpMax = -1;
		
		int foundRes = SendMessageW(_wndRich,EM_FINDTEXTEX,FR_DOWN|FR_MATCHCASE,reinterpret_cast<LPARAM>(&findInfo));
		if (foundRes == -1)
			break;
	
		lastPoint = findInfo.chrgText.cpMax;
		lineCount--;
	}

	//prevent redraws
	SendMessage(_wndRich,WM_SETREDRAW,false,0);

	if (lastPoint > 0)
	{
		//Select found text
		SendMessageW(_wndRich,EM_SETSEL,0,lastPoint);
		//Remove selected text by replacing it with nothing
		SendMessageW(_wndRich,EM_REPLACESEL,false,reinterpret_cast<LPARAM>(L""));
	}

	//set selection to end
	SendMessageW(_wndRich,EM_SETSEL,-1,-1);

	//change the colour
	{
		//default information colour
		COLORREF newColour = RGB(0x33,0x99,0x33);

		if (msg.getPriority() >= Poco::Message::PRIO_DEBUG) //debug, trace
			newColour = RGB(0xBB,0xBB,0xBB); //lightish gray to blend in
		else if (msg.getPriority() == Poco::Message::PRIO_WARNING)
			newColour = RGB(0xFF,0x99,0x33); //orange to get attention
		else if (msg.getPriority() <= Poco::Message::PRIO_ERROR) //bad errors
			newColour = RGB(0xFF,0x00,0x00); //really bright red

		CHARFORMATW colourData;
		memset(&colourData,0,sizeof(colourData));
		colourData.cbSize = sizeof(colourData);
		colourData.dwMask = CFM_COLOR;
		if (msg.getPriority() <= Poco::Message::PRIO_CRITICAL) //really bad errors
		{
			colourData.dwMask |= CFM_BOLD;
			colourData.dwEffects = CFE_BOLD;
		}
		colourData.crTextColor = newColour;

		SendMessageW(_wndRich,EM_SETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&colourData));
	}

	//insert utf8 text
	{
		SETTEXTEX setText;
		setText.flags = ST_SELECTION;
		setText.codepage = 65001;

		SendMessageW(_wndRich,EM_SETTEXTEX,reinterpret_cast<WPARAM>(&setText),reinterpret_cast<LPARAM>(text.c_str()));
	}	

	//enable redraws
	SendMessage(_wndRich,WM_SETREDRAW,true,0);

	//scroll to end
	SendMessageW(_wndRich,EM_SETSEL,-1,-1);
	SendMessageW(_wndRich,EM_SCROLLCARET,0,0);

	//request redraw
	RedrawWindow(_wndRich,nullptr,nullptr,RDW_ERASE|RDW_FRAME|RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);
}