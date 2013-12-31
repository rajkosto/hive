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

#include <iostream>
#include <sstream>

#include "Shared/Common/Types.h"

#include <Poco/Util/ServerApplication.h>
#include <Poco/Path.h>
#include <Poco/UnicodeConverter.h>
#include <Poco/Glob.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

#include <boost/filesystem.hpp>
namespace fs=boost::filesystem;

#include <boost/date_time.hpp>

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <boost/format.hpp>

namespace 
{
	BOOL CALLBACK CloseWindowsProc(HWND hWnd, LPARAM someParam)
	{
		DWORD wantedProcId = GetProcessId((HANDLE)someParam);
		DWORD thisProcId;
		GetWindowThreadProcessId(hWnd, &thisProcId);
		if(thisProcId == wantedProcId)
			PostMessage(hWnd,WM_CLOSE,0,0);

		return TRUE;
	}
};

class RestarterApp : public Poco::Util::ServerApplication
{
private:
	string _srvPath;
	std::map<string,Poco::Util::AbstractConfiguration*> _serverDefs;
	bool _betaSrv;

	mutable boost::mutex _runningCS;
	bool _running;

	bool getRunning() const { boost::mutex::scoped_lock lck(_runningCS); return _running;}
	void setRunning(bool isRunning) { boost::mutex::scoped_lock lck(_runningCS); _running = isRunning; }

	UInt32 _faults;
	mutable boost::mutex _faultsCS;

	UInt32 getFaults() const { boost::mutex::scoped_lock lck(_faultsCS); return _faults; }
	void setFaulted(int serverNum) { boost::mutex::scoped_lock lck(_faultsCS); _faults |= (1 << serverNum); }

	boost::mutex _out;
	boost::mutex _path;

	static void trimPath(string& inputStr)
	{
		using namespace boost::algorithm;
		trim(inputStr);
		trim_if(inputStr,is_any_of("'\""));
		trim(inputStr);
	}
	static string trimPathCopy(string inputStr)
	{
		trimPath(inputStr);
		return inputStr;
	}
	static string escapeQuotes(const string& inputStr)
	{
		string out;
		for(size_t i=0;i<inputStr.length();i++)
		{
			if (inputStr[i] == '"')
				out += "\\";

			out += inputStr[i];
		}
		boost::trim(out);
		return out;
	}
	static int getBuildNum(const string& exePath)
	{
		std::wstring winPath;
		Poco::UnicodeConverter::toUTF16(Poco::Path(Poco::Path::current()).resolve(exePath).toString(),winPath);
		DWORD versionSize = ::GetFileVersionInfoSizeW(winPath.c_str(),NULL);
		if (versionSize < 1)
			return -1;
		vector<UInt8> versionData(versionSize);
		if (::GetFileVersionInfoW(winPath.c_str(),NULL,versionData.size(),(void*)&versionData[0]) == 0)
			return -1;

		VS_FIXEDFILEINFO* infoStruct = NULL;
		UINT structLen = 0;
		if (::VerQueryValueW((const void*)&versionData[0],TEXT("\\"),(void**)&infoStruct,&structLen) == FALSE)
			return -2;

		poco_assert(structLen == sizeof(VS_FIXEDFILEINFO));

		if (infoStruct->dwFileType != VFT_APP)
			return -3;

		return HIWORD(infoStruct->dwProductVersionLS)*1000 + LOWORD(infoStruct->dwProductVersionLS);
	}
	void renameLogs(const string& profilesDirStr)
	{
		_path.lock();
		fs::path profilesDir = Poco::Path(Poco::Path::current()).resolve(profilesDirStr).toString();
		_path.unlock();
		
		string backupFolderName;
		{
			namespace pt=boost::posix_time;
			pt::ptime now = pt::second_clock::local_time();
			boost::format fmt("old_%04u-%02u-%02u_%02u.%02u.%02u");
			fmt % static_cast<int>(now.date().year()) % static_cast<int>(now.date().month()) % static_cast<int>(now.date().day());
			fmt % static_cast<int>(now.time_of_day().hours()) % static_cast<int>(now.time_of_day().minutes()) % static_cast<int>(now.time_of_day().seconds());

			backupFolderName = fmt.str();
		}

		vector<const char*> moveFiles;
		moveFiles.push_back("arma2oaserver.RPT");
		moveFiles.push_back("arma2oaserver.bidmp");
		moveFiles.push_back("arma2oaserver.mdmp");
		moveFiles.push_back("server_console.log");
		moveFiles.push_back("HiveExt.log");

		std::set<string> moreFiles;
		{
			boost::mutex::scoped_lock lck(_path);
			Poco::Glob::glob((profilesDir/"HiveExt.log.*").string(),moreFiles);
		}

		for_each(moreFiles.begin(),moreFiles.end(),[&](const string& fName){
			moveFiles.push_back(fName.c_str());
		});

		bool filesExist = false;
		for_each(moveFiles.begin(),moveFiles.end(),[&](const char* fName){
			if (fs::exists(profilesDir/fName))
				filesExist = true;
		});

		if (filesExist)
		{
			vector<const char*> movedFiles;

			fs::path backupPath = profilesDir/backupFolderName;
			fs::create_directories(backupPath);

			for_each(moveFiles.begin(),moveFiles.end(),[&](const char* fName){
				if (fs::exists(profilesDir/fName))
				{
					fs::rename(profilesDir/fName,backupPath/fName);
					movedFiles.push_back(fName);
				}
			});
			
			if (movedFiles.size() > 0)
			{
				boost::mutex::scoped_lock lck(_out);
				std::cout << "Moved ";
				for(size_t i=0;i<movedFiles.size();i++)
				{
					std::cout << movedFiles[i];
					if (i != movedFiles.size()-1)
						std::cout << " , ";
				}
				std::cout << " to " << backupPath.string() << std::endl;
			}
		}

		string beDirStr = "BattlEye";
		fs::path beDir = profilesDir/beDirStr;
		if (fs::is_directory(beDir))
		{
			//rename the active cfg (if server crashed) to the normal name
			{
				std::set<string> files;
				Poco::Glob::glob((beDir/"BEServer_active_*.cfg").string(),files);
				if (files.size() > 0)
				{
					string badName = *files.begin();
					string goodName = "BEServer.cfg";
					fs::rename(badName,beDir/goodName);

					boost::mutex::scoped_lock lck(_out);
					std::cout << "Renamed " << badName << " back to " << goodName << std::endl;
				}
			}

			string scriptsLogFile = "scripts.log";
			if (fs::exists(beDir/scriptsLogFile))
			{
				fs::path backupPath = beDir/backupFolderName;
				fs::create_directories(backupPath);
				fs::rename(beDir/scriptsLogFile,backupPath/scriptsLogFile);

				boost::mutex::scoped_lock lck(_out);
				std::cout << "Moved " << scriptsLogFile << " to " << backupPath.string() << std::endl;
			}
		}
	}
protected:
	void initialize( Application& self ) override
	{
		ServerApplication::initialize(self);

		//set up working dir for service apps
		if (!this->isInteractive())
		{
			std::wstring setMe;
			Poco::UnicodeConverter::toUTF16(config().getString("application.dir"),setMe);
			::SetCurrentDirectoryW(setMe.c_str());
		}

		loadConfiguration();
		//set up server defs
		{
			Poco::Util::AbstractConfiguration::Keys keys;
			config().keys("",keys);
			for(auto it=keys.begin();it!=keys.end();++it)
			{
				const string& keyName = *it;
				if (!boost::iequals(keyName,"Global") && !boost::iequals(keyName,"application") && !boost::iequals(keyName,"system")) 
					_serverDefs[keyName] = config().createView(keyName);
			}
		}

		_betaSrv = false;
		_srvPath = trimPathCopy(config().getString("Global.exePath",""));
		if (_srvPath.length() > 0)
		{
			int build = getBuildNum(_srvPath);
			if (build < 0)
			{
				std::cerr << "Cannot determine version of " << Poco::Path(Poco::Path::current()).resolve(_srvPath).toString() << " , ignoring" << std::endl;
				_srvPath.clear();
			}
			else 
			{
				Poco::Path newCwd = Poco::Path(Poco::Path::current()).resolve(_srvPath);
				newCwd = newCwd.makeAbsolute();
				if (newCwd.depth() >= 2)
				{
					if (boost::iequals(newCwd.directory(newCwd.depth()-1),"beta") && boost::iequals(newCwd.directory(newCwd.depth()-2),"Expansion"))
					{
						_betaSrv = true;
						newCwd = newCwd.popDirectory().popDirectory();
					}
				}

				newCwd.setFileName("");
				std::wstring setMe;
				Poco::UnicodeConverter::toUTF16(newCwd.toString(),setMe);
				::SetCurrentDirectoryW(setMe.c_str());
			}
		}
	}

	void uninitialize() override
	{
		_srvPath.clear();
		_serverDefs.clear();
		ServerApplication::uninitialize();
	} 

	void serverMonitor(int serverNum, string serverName, Poco::Util::AbstractConfiguration* conf)
	{
		vector<string> startParams;
		startParams.push_back("-nosplash");

		_path.lock();
		fs::path profileFolder = serverName;
		_path.unlock();
		try
		{
			try	
			{ 
				int maxMem = conf->getInt("maxMem");
				startParams.push_back("-maxMem="+lexical_cast<string>(maxMem));
			}
			catch (Poco::NotFoundException) {}

			//set cpuCores
			{
				int defaultNumCores = -1;
				{
					DWORD bufLen = 0;
					GetLogicalProcessorInformation(NULL,&bufLen);
					bufLen /= sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
					vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> logProcs(bufLen);
					bufLen = logProcs.size()*sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
					GetLogicalProcessorInformation(&logProcs[0],&bufLen);
					std::set<ULONG_PTR> uniqMasks;
					for(auto it=logProcs.begin();it!=logProcs.end();++it)
					{
						if (it->Relationship != RelationProcessorCore || it->ProcessorCore.Flags != 1)
							continue;

						uniqMasks.insert(it->ProcessorMask);
					}

					defaultNumCores = uniqMasks.size();
				}

				int cpuCores = conf->getInt("cpuCount",defaultNumCores);
				startParams.push_back("-cpuCount="+lexical_cast<string>(cpuCores));
			}

			try	
			{ 
				int exThreads = conf->getInt("exThreads");
				startParams.push_back("-exThreads="+lexical_cast<string>(exThreads));
			}
			catch (Poco::NotFoundException) {}

			serverName = conf->getString("name",serverName);
			startParams.push_back("-name="+serverName);

			profileFolder = trimPathCopy(conf->getString("profiles",profileFolder.string()));
			startParams.push_back("-profiles="+profileFolder.string());
			if (!fs::exists(profileFolder)) fs::create_directories(profileFolder);

			//set up basic.cfg loc
			try
			{
				fs::path basicLoc = trimPathCopy(conf->getString("cfg"));
				if (!fs::exists(basicLoc))
				{
					boost::mutex::scoped_lock lck(_out);
					std::cerr << serverName << ": Invalid basic.cfg location: " << basicLoc.string() << std::endl;
					throw Poco::SyntaxException();
				}
				startParams.push_back("-cfg="+basicLoc.string());
			}
			catch (Poco::NotFoundException)
			{
				fs::path basicLoc = profileFolder/"arma2.cfg";
				if (!fs::exists(basicLoc))
				{
					basicLoc = profileFolder/"basic.cfg";
					if (!fs::exists(basicLoc))
					{
						basicLoc = profileFolder/"arma2oa.cfg";
						if (!fs::exists(basicLoc))
						{
							boost::mutex::scoped_lock lck(_out);
							std::cerr << serverName << ": No basic.cfg location specified and couldn't find defaults." << std::endl;
							throw Poco::SyntaxException();
						}
					}
				}
				startParams.push_back("-cfg="+basicLoc.string());
			}

			//set up server.cfg loc
			try
			{
				fs::path configLoc = trimPathCopy(conf->getString("config"));
				if (!fs::exists(configLoc))
				{
					boost::mutex::scoped_lock lck(_out);
					std::cerr << serverName << ": Invalid server.cfg location: " << configLoc.string() << std::endl;
					throw Poco::SyntaxException();
				}
				startParams.push_back("-config="+configLoc.string());
			}
			catch (Poco::NotFoundException)
			{
				fs::path configLoc = profileFolder/"server.cfg";
				if (!fs::exists(configLoc))
				{
					boost::mutex::scoped_lock lck(_out);
					std::cerr << serverName << ": No server.cfg location specified and couldn't find defaults." << std::endl;
					throw Poco::SyntaxException();
				}
				startParams.push_back("-config="+configLoc.string());
			}

			try
			{
				string mods = escapeQuotes(conf->getString("mod"));
				if (_betaSrv)
				{
					if (mods.length() > 0 && *mods.rbegin() != ';')
						mods += ";";

					mods += "Expansion\\beta;Expansion\\beta\\Expansion";
				}
				startParams.push_back("-mod="+mods);
			}
			catch (Poco::NotFoundException)	
			{
				if (_betaSrv)
					startParams.push_back("-mod=Expansion\\beta;Expansion\\beta\\Expansion");
			}

			try
			{
				string world = escapeQuotes(conf->getString("world"));
				startParams.push_back("-world="+world);
			}
			catch (Poco::NotFoundException)	{}

			try
			{
				string ipAddr = escapeQuotes(conf->getString("ip"));
				startParams.push_back("-ip="+ipAddr);
			}
			catch (Poco::NotFoundException)	{}

			try
			{
				int port = conf->getInt("port");
				startParams.push_back("-port="+lexical_cast<string>(port));
			}
			catch (Poco::NotFoundException)	{}
		}
		catch(Poco::SyntaxException)
		{
			boost::mutex::scoped_lock lck(_out);
			std::cerr << "Error reading configuration parameters for " << serverName << std::endl;
			setFaulted(serverNum);
			return;
		}

		while (getRunning())
		{
			std::wstring appName;
			Poco::UnicodeConverter::toUTF16(_srvPath,appName);
			std::wstring appParams;
			{
				std::stringstream paramsStr;
				for(auto it=startParams.begin();it!=startParams.end();++it)
				{
					paramsStr << "\"" << *it << "\"";
					if ((startParams.end()-it) > 1)
						paramsStr << " ";
				}
				Poco::UnicodeConverter::toUTF16(paramsStr.str(),appParams);

				boost::mutex::scoped_lock lck(_out);
				std::cout << serverName << ": Starting with params " << paramsStr.str() << std::endl;
			}

			//just in case someone didnt use restarter last time
			renameLogs(profileFolder.string());

			PROCESS_INFORMATION procInfo;
			BOOL goodProc = FALSE;
			{
				STARTUPINFO startInfo;
				memset(&startInfo,0,sizeof(startInfo));
				startInfo.cb = sizeof(startInfo);
				goodProc = ::CreateProcessW(appName.c_str(),const_cast<WCHAR*>(appParams.c_str()),NULL,NULL,FALSE,0,NULL,NULL,&startInfo,&procInfo);
			}
			if (!goodProc)
			{
				boost::mutex::scoped_lock lck(_out);
				std::cerr << "Error creating process for " << serverName << " with error " << GetLastError() << std::endl;
				setFaulted(serverNum);
				return;
			}

			for(;;)
			{
				if (WaitForSingleObject(procInfo.hProcess,100) == WAIT_OBJECT_0) //process terminated
					break;

				if (!getRunning())
				{
					DWORD startTime = GetTickCount();
					for (;;)
					{
						//need to send a close signal to the process
						EnumWindows(CloseWindowsProc,(LPARAM)procInfo.hProcess);
						//wait for the process to gracefully close (up to 60 seconds)
						if (WaitForSingleObject(procInfo.hProcess,1000) == WAIT_OBJECT_0)
							break; //gracefully closed

						//check if we need to force-close
						if ((GetTickCount()-startTime) >= (60*1000))
						{
							boost::mutex::scoped_lock lck(_out);
							std::cerr << serverName << ": Grace period for shutdown expired, TERMINATING." << std::endl;
							TerminateProcess(procInfo.hProcess,0); //forcefully terminate process once 60 seconds are up
							break;
						}
					}

					break;
				}
			}

			//dec ref counts that we took in CreateProcess
			CloseHandle(procInfo.hThread);
			CloseHandle(procInfo.hProcess);

			//rename logs to current date and time (server close date and time)
			renameLogs(profileFolder.string());

			if (getRunning())
			{
				int waitTime = 10;
				{
					boost::mutex::scoped_lock lck(_out);
					std::cout << serverName << ": Waiting " << waitTime << "s before restarting" << std::endl;
				}
				DWORD startMillis = GetTickCount();
				while ((GetTickCount() - startMillis) < (waitTime*1000))
				{
					Sleep(100);
					if (!getRunning())
					{
						boost::mutex::scoped_lock lck(_out);
						std::cout << serverName << ": Restart aborted" << std::endl;
						break;
					}
				}
			}
		}
	}

	int main(const std::vector<std::string>& args)
	{
		int versionNum = -5;
		if (_srvPath.length() < 1)
		{
			string exeName = "arma2oaserver.exe";
			fs::path retailPath = Poco::Path::current();
			fs::path betaPath = retailPath/"Expansion"/"beta";
			retailPath = retailPath/exeName;
			betaPath = betaPath/exeName;

			if (fs::exists(betaPath) && (versionNum=getBuildNum(betaPath.string())) > 0)
			{
				_betaSrv = true;
				_srvPath = betaPath.string();
			}
			else if (fs::exists(retailPath) && (versionNum=getBuildNum(retailPath.string())) > 0)
			{
				_betaSrv = false;
				_srvPath = retailPath.string();
			}
			else
			{
				std::cerr << "Couldn't locate " << exeName << " anywhere!" << std::endl;
				return EXIT_NOINPUT;
			}
		}
		else
			versionNum = getBuildNum(_srvPath);

		//print version
		{
			boost::mutex::scoped_lock lck(_out);
			std::cout << "Using " << _srvPath << " (" << versionNum << ") as server executable" << std::endl;
		}

		if (_serverDefs.size() < 1)
		{
			std::cerr << "No servers defined in config file! Exiting." << std::endl;
			return EXIT_NOINPUT;
		}

		//pre-threads, so dont have to lock cs
		_running = true;
		_faults = 0;

		//set up threads
		vector<boost::thread> threads;
		int serverNum = 0;
		for (auto it=_serverDefs.begin();it!=_serverDefs.end();++it)
		{
			threads.push_back(boost::thread(boost::bind(&RestarterApp::serverMonitor,this,serverNum,it->first,it->second)));
			serverNum++;
		}
		//wait untill we need to close
		waitForTerminationRequest();

		//shutdown notice
		{
			boost::mutex::scoped_lock lck(_out);
			std::cout << "Global: SHUTTING DOWN !!!" << std::endl;
		}
		//closing, set running to false, so that all threads finish within 60sec
		setRunning(false);
		//wait for all threads to finish
		for(auto it=threads.begin();it!=threads.end();++it)
			it->join();

		return EXIT_OK;
	}
};

POCO_SERVER_MAIN(RestarterApp);