/*
* Copyright (C) 2009-2013 Rajko Stojadinovic <http://github.com/rajkosto/hive>
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

#include <Poco/ClassLoader.h>
#include <Poco/Manifest.h>

template <typename Base>
class SharedLibraryLoader
{
public:
	void loadLibrary(const string& libName)
	{
		string fileName = libName + Poco::SharedLibrary::suffix();
		_loader.loadLibrary(fileName);
	}

	Base* create(const std::string& className) const { return _loader.create(className); }
	Base& instance(const std::string& className) const { return _loader.instance(className); }
	bool canCreate(const std::string& className) const { return _loader.canCreate(className); }
	void destroy(const std::string& className, Base* pObject) const { _loader.destroy(className,pObject); }
	bool isAutoDelete(const std::string& className, Base* pObject) const { return _loader.isAutoDelete(className, pObject); }
private:
	typedef Poco::ClassLoader<Base> LibraryLoader;
	LibraryLoader _loader;
};