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

#include "QueryResultPostgre.h"

namespace
{
	//From pg_type.h in postgresql server includes.
	enum PostgreOidType
	{
		BOOLOID			= 16,
		BYTEAOID		= 17,
		CHAROID			= 18,
		NAMEOID			= 19,
		INT8OID			= 20,
		INT2OID			= 21,
		INT2VECTOROID	= 22,
		INT4OID			= 23,
		REGPROCOID		= 24,
		TEXTOID			= 25,
		OIDOID			= 26,
		TIDOID			= 27,
		XIDOID			= 28,
		CIDOID			= 29,
		OIDVECTOROID	= 30,
		POINTOID		= 600,
		LSEGOID			= 601,
		PATHOID			= 602,
		BOXOID			= 603,
		POLYGONOID		= 604,
		LINEOID			= 628,
		FLOAT4OID		= 700,
		FLOAT8OID		= 701,
		ABSTIMEOID		= 702,
		RELTIMEOID		= 703,
		TINTERVALOID	= 704,
		UNKNOWNOID		= 705,
		CIRCLEOID		= 718,
		CASHOID			= 790,
		INETOID			= 869,
		CIDROID			= 650,
		BPCHAROID		= 1042,
		VARCHAROID		= 1043,
		DATEOID			= 1082,
		TIMEOID			= 1083,
		TIMESTAMPOID	= 1114,
		TIMESTAMPTZOID	= 1184,
		INTERVALOID		= 1186,
		TIMETZOID		= 1266,
		BITOID			= 1560,
		VARBITOID		= 1562,
		NUMERICOID		= 1700
	};

	//see types in pg_type.h
	Field::DataTypes PostgreTypeToFieldType(Oid pOid)
	{
		switch (pOid)
		{
			case BPCHAROID:
			case CIDOID:
			case CIDROID:
			case CIRCLEOID:
			case INETOID:
			case NAMEOID:
			case TEXTOID:
			case VARCHAROID:
				return Field::DB_TYPE_STRING;
			case CASHOID:
			case FLOAT4OID:
			case FLOAT8OID:
			case NUMERICOID:
				return Field::DB_TYPE_FLOAT;
			case DATEOID:                                       //Date
			case RELTIMEOID:                                    //Date
			case TIMEOID:                                       //Time
			case TIMETZOID:                                     //Time
			case ABSTIMEOID:                                    //DateTime
			case INTERVALOID:                                   //DateTime
			case TIMESTAMPOID:                                  //DateTime
			case TIMESTAMPTZOID:                                //DateTime
			case INT2OID:                                       //Int
			case INT2VECTOROID:                                 //Int
			case INT4OID:                                       //Int
			case OIDOID:                                        //Int
			case CHAROID:                                       //UInt
			case INT8OID:                                       //LongLong
				return Field::DB_TYPE_INTEGER;
			case BOOLOID:
				return Field::DB_TYPE_BOOL;                     //Bool
	/*
			case BOXOID:    Rect;
			case LINEOID:   Rect;
			case VARBITOID: BitArray;
			case BYTEAOID:  ByteArray;
	*/
			case LSEGOID:
			case OIDVECTOROID:
			case PATHOID:
			case POINTOID:
			case POLYGONOID:
			case REGPROCOID:
			case TIDOID:
			case TINTERVALOID:
			case UNKNOWNOID:
			case XIDOID:
			default:
				return Field::DB_TYPE_UNKNOWN;
		}
		return Field::DB_TYPE_UNKNOWN;
	}
};

QueryResultPostgre::QueryResultPostgre(PostgreSQLConnection* theConn, const char* sql) : _currRes(-1),  _tblIdx(0)
{
	vector<SqlConnection::SqlException> excs;
	for (;;)
	{
		try	
		{ 
			PostgreSQLConnection::ResultInfo resInfo;
			if (theConn->_PostgreStoreResult(sql,&resInfo) == false)
				break;

			_results.push_back(std::move(resInfo)); 
		}
		catch (const SqlConnection::SqlException& e) { excs.push_back(e); }
	}
	if (excs.size() > 0)
		throw excs[0];


	//gotta have at least one result
	poco_assert(nextResult() == true);
}

QueryResultPostgre::~QueryResultPostgre() { }

bool QueryResultPostgre::fetchRow()
{
	//if we're outta results, there's also no more rows
	if (_currRes < 0 || _currRes >= _results.size())
		return false;

	//current result set is in range
	const auto& theRes = _results[_currRes];
	if (theRes.pgRes == nullptr) //not a SELECT
		return false;

	if (_tblIdx >= numRows())
		return false;
	
	for (size_t fieldNum=0; fieldNum<_row.size(); fieldNum++)
	{
		const char* strValue = PQgetvalue(theRes.pgRes,static_cast<int>(_tblIdx),static_cast<int>(fieldNum));
		poco_assert(strValue != nullptr); //postgres always returns a valid cstr pointer
		if (PQgetisnull(theRes.pgRes,static_cast<int>(_tblIdx),static_cast<int>(fieldNum)))
			strValue = nullptr; //nullify if the actual field is NULL

		_row[fieldNum].setValue(strValue);
	}
	_tblIdx++;

	return true;
}

bool QueryResultPostgre::nextResult()
{
	//is the currently selected result in bounds ? if so, free it
	if (_currRes >= 0 && _currRes < _results.size())
		_results[_currRes].clear();
	//select next result
	_currRes++;

	_tblIdx = 0; //reset cursor
	if (_currRes < _results.size())
	{
		const auto& theRes = _results[_currRes];
		setNumFields(theRes.numFields);
		setNumRows(theRes.numRows);

		_row.resize(theRes.numFields);
		if (_row.size() > 0)
		{
			poco_assert(theRes.pgRes != nullptr);
			for (size_t i=0; i<_row.size(); i++)
			{
				_row[i].setValue(nullptr);
				_row[i].setType(PostgreTypeToFieldType(PQftype(theRes.pgRes,static_cast<int>(i))));
			}
		}

		return true;
	}
	else
	{
		_results.clear();
		_currRes = -1;

		setNumFields(0);
		setNumRows(0);
		_row.clear();

		return false;
	}
}

QueryFieldNames QueryResultPostgre::fetchFieldNames() const 
{
	//if we got no result, can't fetch field names
	if (_currRes < 0 || _currRes >= _results.size())
		return QueryFieldNames();

	const auto& theRes = _results[_currRes];

	QueryFieldNames fieldNames(theRes.numFields);
	if (fieldNames.size() > 0)
	{
		poco_assert(theRes.pgRes != nullptr);
		for (size_t i=0; i<fieldNames.size(); i++)
			fieldNames[i] = PQfname(theRes.pgRes,i);
	}


	return std::move(fieldNames);
}

