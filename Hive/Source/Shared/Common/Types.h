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

#define NOMINMAX

#include <Poco/Types.h>

using Poco::Int8;
using Poco::Int16; 
using Poco::Int32; 
using Poco::Int64;
using Poco::UInt8;
using Poco::UInt16; 
using Poco::UInt32; 
using Poco::UInt64;
using Poco::IntPtr;
using Poco::UIntPtr;

#include <memory>
using std::unique_ptr;
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;
#include <boost/weak_ptr.hpp>
using boost::weak_ptr;
#include <boost/make_shared.hpp>
using boost::make_shared;
#include <boost/scoped_ptr.hpp>
using boost::scoped_ptr;

#include <string>
using std::string;
#include <vector>
using std::vector;
typedef std::vector<UInt8> ByteVector;
#include <map>
using std::map;
#include <boost/unordered_map.hpp>
using boost::unordered_map;
#include <deque>
using std::deque;
#include <queue>
using std::queue;
#include <list>
using std::list;

using std::begin;
using std::end;
using std::for_each;

#include <boost/array.hpp>
using boost::array;