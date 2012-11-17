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
#include "Shared/Common/Exception.h"
#include <sstream>

union SqlStmtField
{
	bool boolean;
	UInt8 ui8;
	Int8 i8;
	UInt16 ui16;
	Int16 i16;
	UInt32 ui32;
	Int32 i32;
	UInt64 ui64;
	Int64 i64;
	float f;
	double d;
};

enum SqlStmtFieldType
{
	FIELD_BOOL,
	FIELD_UI8,
	FIELD_UI16,
	FIELD_UI32,
	FIELD_UI64,
	FIELD_I8,
	FIELD_I16,
	FIELD_I32,
	FIELD_I64,
	FIELD_FLOAT,
	FIELD_DOUBLE,
	FIELD_STRING,
	FIELD_BINARY,
	FIELD_NONE
};

//templates might be the best choice here
//but I didn't have time to play with them
class SqlStmtFieldData
{
public:
	SqlStmtFieldData() : m_type(FIELD_NONE) { m_numberData.ui64 = 0; }
	~SqlStmtFieldData() {}

	template<typename T>
	SqlStmtFieldData(T param) { set(param); }

	SqlStmtFieldData(const UInt8* data, size_t size) { set(data,size); }
	SqlStmtFieldData(const char* str, size_t size) { set(str,size); }

	template<typename T1>
	void set(T1 param1);

	void set(const std::string& val);
	void set(const ByteVector& val);
	void set(const UInt8* data, size_t size);
	void set(const char* str, size_t strSize);

	//getters
	bool toBool() const { poco_assert(m_type == FIELD_BOOL); return (m_numberData.ui8)? true:false; }
	UInt8 toUint8() const { poco_assert(m_type == FIELD_UI8); return m_numberData.ui8; }
	Int8 toInt8() const { poco_assert(m_type == FIELD_I8); return m_numberData.i8; }
	UInt16 toUint16() const { poco_assert(m_type == FIELD_UI16); return m_numberData.ui16; }
	Int16 toInt16() const { poco_assert(m_type == FIELD_I16); return m_numberData.i16; }
	UInt32 toUint32() const { poco_assert(m_type == FIELD_UI32); return m_numberData.ui32; }
	Int32 toInt32() const { poco_assert(m_type == FIELD_I32); return m_numberData.i32; }
	UInt64 toUint64() const { poco_assert(m_type == FIELD_UI64); return m_numberData.ui64; }
	Int64 toInt64() const { poco_assert(m_type == FIELD_I64); return m_numberData.i64; }
	float toFloat() const { poco_assert(m_type == FIELD_FLOAT); return m_numberData.f; }
	double toDouble() const { poco_assert(m_type == FIELD_DOUBLE); return m_numberData.d; }
	std::string toString() const { poco_assert(m_type == FIELD_STRING); return m_stringData; }
	const char* toCStr() const { return this->toString().c_str(); }
	ByteVector toVector() const { poco_assert(m_type == FIELD_BINARY); return m_vectData; }

	//get type of data
	SqlStmtFieldType type() const { return m_type; }
	//get underlying buffer type
	void* buff() const
	{ 
		if (m_type == FIELD_BINARY)
		{
			if (m_vectData.size() > 0)
				return (void*)&m_vectData[0];
			else
				return NULL;
		}
		else if (m_type == FIELD_STRING)
			return (void*)m_stringData.c_str();
		else
			return (void*)&m_numberData; 
	}

	//get size of data
	size_t size() const
	{
		switch (m_type)
		{
		case FIELD_NONE:    return 0;
		case FIELD_BOOL:    //return sizeof(bool);
		case FIELD_UI8:     return sizeof(UInt8);
		case FIELD_UI16:    return sizeof(UInt16);
		case FIELD_UI32:    return sizeof(UInt32);
		case FIELD_UI64:    return sizeof(UInt64);
		case FIELD_I8:      return sizeof(Int8);
		case FIELD_I16:     return sizeof(Int16);
		case FIELD_I32:     return sizeof(Int32);
		case FIELD_I64:     return sizeof(Int64);
		case FIELD_FLOAT:   return sizeof(float);
		case FIELD_DOUBLE:  return sizeof(double);
		case FIELD_STRING:  return m_stringData.length();
		case FIELD_BINARY:	return m_vectData.size();

		default:
			throw std::runtime_error("unrecognized type of SqlStmtFieldType obtained");
		}
	}

private:
	SqlStmtFieldType m_type;
	SqlStmtField m_numberData;
	std::string m_stringData;
	ByteVector m_vectData;
};

//template specialization
template<> inline void SqlStmtFieldData::set(bool val) { m_type = FIELD_BOOL; m_numberData.ui8 = val; }
template<> inline void SqlStmtFieldData::set(UInt8 val) { m_type = FIELD_UI8; m_numberData.ui8 = val; }
template<> inline void SqlStmtFieldData::set(Int8 val) { m_type = FIELD_I8; m_numberData.i8 = val; }
template<> inline void SqlStmtFieldData::set(UInt16 val) { m_type = FIELD_UI16; m_numberData.ui16 = val; }
template<> inline void SqlStmtFieldData::set(Int16 val) { m_type = FIELD_I16; m_numberData.i16 = val; }
template<> inline void SqlStmtFieldData::set(UInt32 val) { m_type = FIELD_UI32; m_numberData.ui32 = val; }
template<> inline void SqlStmtFieldData::set(Int32 val) { m_type = FIELD_I32; m_numberData.i32 = val; }
template<> inline void SqlStmtFieldData::set(UInt64 val) { m_type = FIELD_UI64; m_numberData.ui64 = val; }
template<> inline void SqlStmtFieldData::set(Int64 val) { m_type = FIELD_I64; m_numberData.i64 = val; }
template<> inline void SqlStmtFieldData::set(float val) { m_type = FIELD_FLOAT; m_numberData.f = val; }
template<> inline void SqlStmtFieldData::set(double val) { m_type = FIELD_DOUBLE; m_numberData.d = val; }
template<> inline void SqlStmtFieldData::set(const char* val) { m_type = FIELD_STRING; m_stringData = val; }

inline void SqlStmtFieldData::set(const std::string& val) { m_type = FIELD_STRING; m_stringData = val; }
inline void SqlStmtFieldData::set(const ByteVector& val) { m_type = FIELD_BINARY; m_vectData = val; }
inline void SqlStmtFieldData::set(const UInt8* data, size_t size) 
{
	m_type = FIELD_BINARY; 
	m_vectData.resize(size);
	if (size>0)
		std::copy(data,data+size,m_vectData.begin());
}
inline void SqlStmtFieldData::set(const char* str, size_t strSize)
{
	m_type = FIELD_STRING;
	if (strSize>0)
		m_stringData = std::string(str,strSize);
	else
		m_stringData = std::string();
}

class SqlStatement;
//prepared statement executor
class SqlStmtParameters
{
public:
	typedef std::vector<SqlStmtFieldData> ParameterContainer;

	//reserve memory to contain all input parameters of stmt
	explicit SqlStmtParameters(size_t nParams)
	{
		//reserve memory if needed
		if(nParams > 0)
			m_params.reserve(nParams);
	}

	~SqlStmtParameters() 
	{
	}

	//get amount of bound parameters
	size_t boundParams() const 
	{
		return m_params.size();
	}
	//add parameter
	void addParam(const SqlStmtFieldData& data) 
	{
		m_params.push_back(data); 
	}
	//empty SQL statement parameters. In case nParams > 1 - reserve memory for parameters
	//should help to reuse the same object with batched SQL requests
	void reset( size_t numArguments = 0 )
	{
		m_params.clear();
		//reserve memory if needed
		if(numArguments > 0)
			m_params.reserve(numArguments);
	}
	//swaps contents of internal param container
	void swap(SqlStmtParameters& obj)
	{
		std::swap(m_params, obj.m_params);
	}
	//get bound parameters
	const ParameterContainer& params() const 
	{
		return m_params; 
	}

private:
	SqlStmtParameters& operator=(const SqlStmtParameters& obj);

	//statement parameter holder
	ParameterContainer m_params;
};

//statement ID encapsulation logic
class SqlStatementID
{
public:
	SqlStatementID() : _id(0), _numArgs(0) {}

	UInt32 getId() const { return _id; }
	size_t numArgs() const { return _numArgs; }
	bool isInitialized() const { return (_id != 0); }
private:
	friend class ConcreteDatabase;
	void init(UInt32 id, size_t nArgs) { _id = id; _numArgs = nArgs; }

	UInt32 _id;
	size_t _numArgs;
};

//statement index
class SqlStatement
{
public:
	virtual ~SqlStatement() { delete m_pParams; }

	UInt32 getId() const { return m_index.getId(); }
	size_t numArgs() const { return m_index.numArgs(); }

	virtual bool Execute() = 0;
	virtual bool DirectExecute() = 0;

	//templates to simplify 1-5 parameter bindings
	template<typename ParamType1>
	bool PExecute(ParamType1 param1)
	{
		arg(param1);
		return Execute();
	}

	template<typename ParamType1, typename ParamType2>
	bool PExecute(ParamType1 param1, ParamType2 param2)
	{
		arg(param1);
		arg(param2);
		return Execute();
	}

	template<typename ParamType1, typename ParamType2, typename ParamType3>
	bool PExecute(ParamType1 param1, ParamType2 param2, ParamType3 param3)
	{
		arg(param1);
		arg(param2);
		arg(param3);
		return Execute();
	}

	template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
	bool PExecute(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4)
	{
		arg(param1);
		arg(param2);
		arg(param3);
		arg(param4);
		return Execute();
	}

	template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4, typename ParamType5>
	bool PExecute(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4, ParamType5 param5)
	{
		arg(param1);
		arg(param2);
		arg(param3);
		arg(param4);
		arg(param5);
		return Execute();
	}

	//bind parameters with specified type
	void addBool(bool var) { arg(var); }
	void addUInt8(UInt8 var) { arg(var); }
	void addInt8(Int8 var) { arg(var); }
	void addUInt16(UInt16 var) { arg(var); }
	void addInt16(Int16 var) { arg(var); }
	void addUInt32(UInt32 var) { arg(var); }
	void addInt32(Int32 var) { arg(var); }
	void addUInt64(UInt64 var) { arg(var); }
	void addInt64(Int64 var) { arg(var); }
	void addFloat(float var) { arg(var); }
	void addDouble(double var) { arg(var); }
	void addString(const char* var) { arg(var); }
	void addString(const std::string& var) { arg(var); }
	void addString(std::ostringstream& ss) { arg(ss.str()); ss.str(std::string()); }
	void addString(const char* var, size_t size)  { arg(var,size); }
	void addBinary(const UInt8* data, size_t size) { arg(data,size); }
	void addBinary(const ByteVector& data) { arg(data); }
private:
	SqlStmtParameters* get()
	{
		if(!m_pParams)
			m_pParams = new SqlStmtParameters(numArgs());

		return m_pParams;
	}
	//helper function
	//use appropriate add* functions to bind specific data type
	template<typename ParamType>
	void arg(ParamType val)
	{
		SqlStmtParameters* p = get();
		p->addParam(SqlStmtFieldData(val));
	}
	template<typename ParamType1, typename ParamType2>
	void arg(ParamType1 val1, ParamType2 val2)
	{
		SqlStmtParameters* p = get();
		p->addParam(SqlStmtFieldData(val1,val2));
	}
protected:
	SqlStatementID m_index;
	SqlStmtParameters* m_pParams;
};