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
#include <Poco/NumberParser.h>

class Field
{
public:
	enum DataTypes
	{
		DB_TYPE_UNKNOWN = 0x00,
		DB_TYPE_STRING  = 0x01,
		DB_TYPE_INTEGER = 0x02,
		DB_TYPE_FLOAT   = 0x03,
		DB_TYPE_BOOL    = 0x04
	};

	Field() : mValue(NULL), mType(DB_TYPE_UNKNOWN) {}
	Field(const char* value, enum DataTypes type) : mValue(value), mType(type) {}
	~Field() {}

	enum DataTypes GetType() const { return mType; }
	bool IsNULL() const { return mValue == NULL; }

	const char* GetString() const { return mValue; }
	std::string GetCppString() const
	{
		return mValue ? mValue : "";                    // std::string s = 0 has undefined result in C++
	}
	double GetDouble() const { return mValue ? static_cast<double>(atof(mValue)) : 0.0; }
	float GetFloat() const { return static_cast<float>(GetDouble()); }
	bool GetBool() const { return mValue ? atoi(mValue) > 0 : false; }
	Int32 GetInt32() const { return mValue ? static_cast<Int32>(atol(mValue)) : Int32(0); }
	Int8 GetInt8() const { return mValue ? static_cast<Int8>(atol(mValue)) : Int8(0); }
	UInt8 GetUInt8() const { return mValue ? static_cast<UInt8>(atol(mValue)) : UInt8(0); }
	UInt16 GetUInt16() const { return mValue ? static_cast<UInt16>(atol(mValue)) : UInt16(0); }
	Int16 GetInt16() const { return mValue ? static_cast<Int16>(atol(mValue)) : Int16(0); }
	UInt32 GetUInt32() const { return mValue ? static_cast<UInt32>(atol(mValue)) : UInt32(0); }
	UInt64 GetUInt64() const
	{
		if (!mValue)
			return 0;

		UInt64 parsedVal;
		if (!Poco::NumberParser::tryParseUnsigned64(mValue,parsedVal))
			return 0;

		return parsedVal;
	}


	void SetType(enum DataTypes type) { mType = type; }
	//no need for memory allocations to store resultset field strings
	//all we need is to cache pointers returned by different DBMS APIs
	void SetValue(const char* value) { mValue = value; };

private:
	Field(Field const&);
	Field& operator=(Field const&);

	const char* mValue;
	enum DataTypes mType;
};