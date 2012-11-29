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
#include <boost/variant.hpp>
#include <sstream>

class SqlStmtField
{
public:
	enum Type
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
		FIELD_COUNT
	};

	template<typename T>
	SqlStmtField(T param) { set(param); }

	SqlStmtField(const UInt8* data, size_t size) { set(data,size); }
	SqlStmtField(const char* str, size_t size) { set(str,size); }

	template<typename T1>
	void set(T1 param1);

	void set(std::string val);
	void set(ByteVector val);
	void set(const UInt8* data, size_t size);
	void set(const char* str, size_t strSize);

	//getters
	bool				toBool() const		{ return boost::get<bool>(_data); }
	UInt8				toUint8() const		{ return boost::get<UInt8>(_data); }
	Int8				toInt8() const		{ return boost::get<Int8>(_data); }
	UInt16				toUint16() const	{ return boost::get<UInt16>(_data); }
	Int16				toInt16() const		{ return boost::get<Int16>(_data); }
	UInt32				toUint32() const	{ return boost::get<UInt32>(_data); }
	Int32				toInt32() const		{ return boost::get<Int32>(_data); }
	UInt64				toUint64() const	{ return boost::get<UInt64>(_data); }
	Int64				toInt64() const		{ return boost::get<Int64>(_data); }
	float				toFloat() const		{ return boost::get<float>(_data); }
	double				toDouble() const	{ return boost::get<double>(_data); }
	const std::string&	toString() const	{ return boost::get<const std::string&>(_data); }
	const char*			toCStr() const		{ return toString().c_str(); }
	const ByteVector&	toVector() const	{ return boost::get<const ByteVector&>(_data); }

	//get type of data
	Type type() const 
	{ 
		struct TypeVisitor : public boost::static_visitor<Type>
		{
			Type operator ()(bool) const				{ return FIELD_BOOL; }
			Type operator ()(UInt8) const				{ return FIELD_UI8; }
			Type operator ()(Int8) const				{ return FIELD_I8; }
			Type operator ()(UInt16) const				{ return FIELD_UI16; }
			Type operator ()(Int16) const				{ return FIELD_I16; }
			Type operator ()(UInt32) const				{ return FIELD_UI32; }
			Type operator ()(Int32) const				{ return FIELD_I32; }
			Type operator ()(UInt64) const				{ return FIELD_UI64; }
			Type operator ()(Int64) const				{ return FIELD_I64; }
			Type operator ()(float) const				{ return FIELD_FLOAT; }
			Type operator ()(double) const				{ return FIELD_DOUBLE; }
			Type operator ()(const std::string&) const	{ return FIELD_STRING; }
			Type operator ()(const ByteVector&) const	{ return FIELD_BINARY; }
		};

		return boost::apply_visitor(TypeVisitor(),_data);
	}
	//get pointer to underlying data
	const void* buff() const
	{ 
		struct BufferVisitor : public boost::static_visitor<const void*>
		{
			const void* operator ()(const bool& data) const			{ return &data; }
			const void* operator ()(const UInt8& data) const		{ return &data; }
			const void* operator ()(const Int8& data) const			{ return &data; }
			const void* operator ()(const UInt16& data) const		{ return &data; }
			const void* operator ()(const Int16& data) const		{ return &data; }
			const void* operator ()(const UInt32& data) const		{ return &data; }
			const void* operator ()(const Int32& data) const		{ return &data; }
			const void* operator ()(const UInt64& data) const		{ return &data; }
			const void* operator ()(const Int64& data) const		{ return &data; }
			const void* operator ()(const float& data) const		{ return &data; }
			const void* operator ()(const double& data) const		{ return &data; }
			const void* operator ()(const std::string& data) const	{ return data.c_str(); }
			const void* operator ()(const ByteVector& data) const	
			{ 
				if (data.size() > 0)
					return &data[0];

				return nullptr;
			}
		};

		return boost::apply_visitor(BufferVisitor(), _data);
	}

	//get size of data
	size_t size() const
	{
		struct SizeVisitor : public boost::static_visitor<size_t>
		{
			size_t operator ()(bool) const						{ return sizeof(bool); }
			size_t operator ()(UInt8) const						{ return sizeof(UInt8); }
			size_t operator ()(Int8) const						{ return sizeof(Int8); }
			size_t operator ()(UInt16) const					{ return sizeof(UInt16); }
			size_t operator ()(Int16) const						{ return sizeof(Int16); }
			size_t operator ()(UInt32) const					{ return sizeof(UInt32); }
			size_t operator ()(Int32) const						{ return sizeof(Int32); }
			size_t operator ()(UInt64) const					{ return sizeof(UInt64); }
			size_t operator ()(Int64) const						{ return sizeof(Int64); }
			size_t operator ()(float) const						{ return sizeof(float); }
			size_t operator ()(double) const					{ return sizeof(double); }
			size_t operator ()(const std::string& data) const	{ return data.length(); }
			size_t operator ()(const ByteVector& data) const	{ return data.size(); }
		};

		return boost::apply_visitor(SizeVisitor(),_data);
	}

private:
	typedef boost::variant< std::string, ByteVector, 
		bool, UInt8, UInt16, UInt32, UInt64, 
		Int8, Int16, Int32, Int64, float, double > FieldVariant;

	FieldVariant _data;
};

//template specialization
template<> inline void SqlStmtField::set(bool val) { _data = val; }
template<> inline void SqlStmtField::set(UInt8 val) { _data = val; }
template<> inline void SqlStmtField::set(Int8 val) { _data = val; }
template<> inline void SqlStmtField::set(UInt16 val) { _data = val; }
template<> inline void SqlStmtField::set(Int16 val) { _data = val; }
template<> inline void SqlStmtField::set(UInt32 val) { _data = val; }
template<> inline void SqlStmtField::set(Int32 val) { _data = val; }
template<> inline void SqlStmtField::set(UInt64 val) { _data = val; }
template<> inline void SqlStmtField::set(Int64 val) { _data = val; }
template<> inline void SqlStmtField::set(float val) { _data = val; }
template<> inline void SqlStmtField::set(double val) { _data = val; }
template<> inline void SqlStmtField::set(const char* val) { _data = std::string(val); }

inline void SqlStmtField::set(std::string val) { _data = std::move(val); }
inline void SqlStmtField::set(ByteVector val) { _data = std::move(val); }
inline void SqlStmtField::set(const UInt8* data, size_t size) 
{
	ByteVector localData(size);
	if (size>0)
		std::copy(data,data+size,localData.begin());
	_data = std::move(localData);
}
inline void SqlStmtField::set(const char* str, size_t strSize)
{
	if (strSize>0)
		_data = std::string(str,strSize);
	else
		_data = std::string();
}

class SqlStatement;
//prepared statement executor
class SqlStmtParameters
{
public:
	typedef std::vector<SqlStmtField> ParameterContainer;

	//reserve memory to contain all input parameters of stmt
	void reserve(size_t numParams)
	{
		if(numParams > 0)
		{
			if (numParams > _params.capacity())
				_params.reserve(numParams);
		}
	}

	//get amount of bound parameters
	size_t boundParams() const 
	{
		return _params.size();
	}
	//add parameter
	void addParam(const SqlStmtField& data) 
	{
		_params.push_back(data); 
	}
	//empty SQL statement parameters. In case nParams > 0 - reserve memory for parameters
	//should help to reuse the same object with batched SQL requests
	void reset(size_t numArguments = 0)
	{
		_params.clear();
		//reserve memory if needed
		if(numArguments > 0)
			_params.reserve(numArguments);
	}
	//swaps contents of internal param container
	void swap(SqlStmtParameters& obj)
	{
		std::swap(_params, obj._params);
	}
	//get bound parameters
	const ParameterContainer& params() const 
	{
		return _params; 
	}

private:
	//statement parameter holder
	ParameterContainer _params;
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
	virtual ~SqlStatement() {}

	UInt32 getId() const { return _stmtId.getId(); }
	size_t numArgs() const { return _stmtId.numArgs(); }

	virtual bool execute() = 0;
	virtual bool directExecute() = 0;

	//templates to simplify 1-5 parameter bindings
	template<typename ParamType1>
	bool executeParams(ParamType1 param1)
	{
		arg(param1);
		return execute();
	}

	template<typename ParamType1, typename ParamType2>
	bool executeParams(ParamType1 param1, ParamType2 param2)
	{
		arg(param1);
		arg(param2);
		return execute();
	}

	template<typename ParamType1, typename ParamType2, typename ParamType3>
	bool executeParams(ParamType1 param1, ParamType2 param2, ParamType3 param3)
	{
		arg(param1);
		arg(param2);
		arg(param3);
		return execute();
	}

	template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
	bool executeParams(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4)
	{
		arg(param1);
		arg(param2);
		arg(param3);
		arg(param4);
		return execute();
	}

	template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4, typename ParamType5>
	bool executeParams(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4, ParamType5 param5)
	{
		arg(param1);
		arg(param2);
		arg(param3);
		arg(param4);
		arg(param5);
		return execute();
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
	void addString(std::string var) { arg(std::move(var)); }
	void addString(std::ostringstream& ss) { arg(ss.str()); ss.str(std::string()); }
	void addString(const char* var, size_t size)  { arg(var,size); }
	void addBinary(const UInt8* data, size_t size) { arg(data,size); }
	void addBinary(ByteVector data) { arg(std::move(data)); }
private:
	SqlStmtParameters& get()
	{
		_params.reserve(numArgs());		
		return _params;
	}
	//helper function
	//use appropriate add* functions to bind specific data type
	template<typename ParamType>
	void arg(ParamType val)
	{
		get().addParam(SqlStmtField(val));
	}
	template<typename ParamType1, typename ParamType2>
	void arg(ParamType1 val1, ParamType2 val2)
	{
		get().addParam(SqlStmtField(val1,val2));
	}
protected:
	SqlStatementID _stmtId;
	SqlStmtParameters _params;
};