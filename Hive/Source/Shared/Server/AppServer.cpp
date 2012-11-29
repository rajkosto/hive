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

#include "AppServer.h"
#include "Log/ArmaConsoleChannel.h"
#include "Log/HiveConsoleChannel.h"

#include <Poco/ConsoleChannel.h>
#include <Poco/FileChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/FormattingChannel.h>
#include <Poco/SplitterChannel.h>
#include <Poco/AsyncChannel.h>
#include <Poco/String.h>
#include <Poco/Path.h>

void AppServer::initialize( Application& self )
{
	ServerApplication::initialize(self);

	if (appDir.length() < 1)
		appDir = config().getString("application.dir");
	else
	{
		auto cwd = Poco::Path(Poco::Path::current());
		cwd.resolve(Poco::Path(appDir));
		appDir = cwd.toString();
		if (appDir[appDir.length()-1] != Poco::Path::separator())
			appDir += Poco::Path::separator();
	}

	if (appName.length() < 1)
		appName = config().getString("application.baseName");

	initConfig();
	initLogger();
}

void AppServer::uninitialize()
{
	ServerApplication::uninitialize();
}

#include <iostream>

void AppServer::initConfig()
{
	try
	{
		loadConfiguration(appDir+appName+std::string(".ini"));
	}
	catch(const Poco::IOException& e)
	{
		std::cout << "Unable to load configuration: " << e.displayText() << std::endl;
	}
}

void AppServer::initLogger()
{
	using Poco::AutoPtr;
	using Poco::ConsoleChannel;
	using Poco::FileChannel;
	using Poco::SplitterChannel;
	using Poco::FormattingChannel;
	using Poco::PatternFormatter;
	using Poco::Util::AbstractConfiguration;
	using Poco::Logger;

	AutoPtr<AbstractConfiguration> logConf(config().createView("Logger"));
	AutoPtr<SplitterChannel> splitChan(new SplitterChannel);

	//Set up the file channel
	{
		AutoPtr<FileChannel> fileChan(new FileChannel);
		fileChan->setProperty("path", appDir+logConf->getString("Filename",appName+std::string(".log")) );
		fileChan->setProperty("rotation", logConf->getString("Rotation","never") );
		fileChan->setProperty("archive", "timestamp");
		fileChan->setProperty("times", "local");

		AutoPtr<PatternFormatter> fileFormatter(new PatternFormatter);
		fileFormatter->setProperty("pattern", logConf->getString("FilePattern","%Y-%m-%d %H:%M:%S %s: [%p] %t"));
		fileFormatter->setProperty("times", "local");

		AutoPtr<FormattingChannel> fileFormatChan(new FormattingChannel(fileFormatter, fileChan));
		splitChan->addChannel(fileFormatChan);
	}
	//Set up the console channel
	{
		bool useRealConsole = true;
#ifndef _DEBUG
		useRealConsole = logConf->getBool("SeparateConsole",false);
#endif
		AutoPtr<CustomLevelChannel> consoleChan;
		if (useRealConsole)
		{
			string title = appName;
			if (appDir.length() > 1)
			{
				Poco::Path dirPath(appDir);
				if (dirPath.depth() > 0)
					title += " - " + dirPath[dirPath.depth()-1];
			}

			consoleChan = new HiveConsoleChannel(std::move(title));
		}
		else
			consoleChan = new ArmaConsoleChannel;

		if (logConf->hasProperty("ConsoleLevel"))
			consoleChan->overrideLevel(Poco::Logger::parseLevel(logConf->getString("ConsoleLevel")));

		AutoPtr<PatternFormatter> consoleFormatter(new PatternFormatter);
		consoleFormatter->setProperty("pattern", logConf->getString("ConsolePattern","%H:%M:%S %s(%I): [%p] %t") );
		consoleFormatter->setProperty("times", "local");

		AutoPtr<FormattingChannel> consFormatChan(new FormattingChannel(consoleFormatter, consoleChan));
		splitChan->addChannel(consFormatChan);
	}
	Logger::root().setChannel(splitChan);

	std::string loggingLevel = Poco::toLower(logConf->getString("Level","information"));
	Logger::root().setLevel(loggingLevel);

	this->setLogger(Logger::get(appName));
}

void AppServer::enableAsyncLogging()
{
	using Poco::Logger;
	using Poco::AutoPtr;
	using Poco::SplitterChannel;
	using Poco::AsyncChannel;	

	SplitterChannel* splitChan = dynamic_cast<SplitterChannel*>(Logger::root().getChannel());
	if (splitChan != nullptr) //only if its not async already
	{
		//make it async
		AutoPtr<AsyncChannel> asyncChan(new AsyncChannel(splitChan));
		Logger::setChannel("",asyncChan);
	}
}
