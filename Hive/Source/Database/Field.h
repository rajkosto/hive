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

	Field() : _value(nullptr), _type(DB_TYPE_UNKNOWN) {}
	Field(const char* value, enum DataTypes type) : _value(value), _type(type) {}
	~Field() {}

	DataTypes getType() const { return _type; }
	bool isNull() const { return _value == nullptr; }

	const char* getCStr() const { return _value; }
	std::string getString() const
	{
		//std::string s = 0 has undefined result
		return _value ? _value : "";
	}
	double getDouble() const { return _value ? static_cast<double>(atof(_value)) : 0.0; }
	float getFloat() const { return static_cast<float>(getDouble()); }
	bool getBool() const { return _value ? atoi(_value) > 0 : false; }
	Int32 getInt32() const { return _value ? static_cast<Int32>(atol(_value)) : Int32(0); }
	Int8 getInt8() const { return _value ? static_cast<Int8>(atol(_value)) : Int8(0); }
	UInt8 getUInt8() const { return _value ? static_cast<UInt8>(atol(_value)) : UInt8(0); }
	UInt16 getUInt16() const { return _value ? static_cast<UInt16>(atol(_value)) : UInt16(0); }
	Int16 getInt16() const { return _value ? static_cast<Int16>(atol(_value)) : Int16(0); }
	UInt32 getUInt32() const { return _value ? static_cast<UInt32>(atol(_value)) : UInt32(0); }
	UInt64 getUInt64() const
	{
		if (!_value)
			return 0;

		UInt64 parsedVal;
		if (!Poco::NumberParser::tryParseUnsigned64(_value,parsedVal))
			return 0;

		return parsedVal;
	}

	void setType(DataTypes type) { _type = type; }
	//no need for memory allocations to store resultset field strings
	//all we need is to cache pointers returned by different DBMS APIs
	void setValue(const char* value) { _value = value; };
private:
	const char* _value;
	enum DataTypes _type;
};