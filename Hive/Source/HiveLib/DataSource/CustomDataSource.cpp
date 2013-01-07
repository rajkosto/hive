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

#include "CustomDataSource.h"
#include "Database/Database.h"
#include <Poco/Util/AbstractConfiguration.h>
#include <Poco/RandomStream.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>

bool CustomDataSource::TableInfo::operator==( const TableInfo& rhs ) const
{
	if (this->dbase != rhs.dbase)
		return false;

	return boost::iequals(this->table,rhs.table);
}

CustomDataSource::TableInfo::TableInfo( std::string inputName )
{
	boost::trim(inputName);
	if (inputName.length() < 1)
		throw CustomDataSource::InvalidTableException(inputName,"TableInfo empty");

	string dbName;
	string tableName;
	if (inputName.length() > 0)
	{
		auto dotPos = inputName.find_first_of('.');
		if (dotPos != string::npos)
		{
			dbName = inputName.substr(0,dotPos);
			if (dotPos < inputName.length()-1)
				tableName = inputName.substr(dotPos+1);
		}
		else
			tableName = inputName;
	}

	boost::trim(dbName);
	boost::trim(tableName);
	boost::to_lower(dbName);	

	DbSource dbType = DB_UNK;
	{
		static const string charName = "char";
		static const string objName = "obj";
		if (dbName.length() >= charName.length())
		{
			if (dbName.substr(0,charName.length()) == charName)
				dbType = DB_CHAR;
		}	
		if (dbType == DB_UNK && dbName.length() >= objName.length())
		{
			if (dbName.substr(0,objName.length()) == objName)
				dbType = DB_OBJ;
		}
	}

	if (dbType >= DB_UNK)
		throw CustomDataSource::InvalidTableException(inputName,"TableInfo has unknown Database");

	if (tableName.length() < 1)
		throw CustomDataSource::InvalidTableException(inputName,"TableInfo has empty Table Name");

	this->dbase = dbType;
	this->table = tableName;
}

CustomDataSource::CustomDataSource( Poco::Logger& logger, shared_ptr<Database> charDb, shared_ptr<Database> objDb ) : DataSource(logger)
{
	_dbs[DB_CHAR] = charDb;
	_dbs[DB_OBJ] = objDb;

	//use a strong crypto random source to seed the PRNG
	{
		UInt32 rngSeed = 0;
		Poco::RandomInputStream().read((char*)&rngSeed,sizeof(rngSeed));
		poco_assert(rngSeed != 0);
		_rng.seed(rngSeed);
	}
}

void CustomDataSource::VerifyTable( string whichOne )
{
	TableInfo tblInfo(whichOne);
	poco_assert(tblInfo.isValid());
}

bool CustomDataSource::allowTable( string whichOne )
{
	TableInfo tblInfo(whichOne);

	if (std::find(_allowed.begin(),_allowed.end(),tblInfo) != _allowed.end())
		return false;

	_allowed.push_back(std::move(tblInfo));
	return true;
}

bool CustomDataSource::removeAllowedTable( string whichOne )
{
	TableInfo tblInfo(whichOne);

	auto it = std::find(_allowed.begin(),_allowed.end(),tblInfo);
	if (it != _allowed.end())
	{
		_allowed.erase(it);
		return true;
	}

	return false;
}

vector<string> CustomDataSource::getAllowedTables() const
{
	vector<string> retVal(_allowed.size());
	for (size_t i=0; i<retVal.size(); i++)
		retVal[i] = _allowed[i].toString();

	return retVal;
}

CustomDataSource::~CustomDataSource() {}

CustomDataSource::WhereCond::WhereCond( string column_, Operand operand_, string constant_ ) 
	: column(std::move(column_)), lengthOf(false), operand(operand_), constant(std::move(constant_))
{
	boost::trim(column);
	if (column.length() > 0)
	{
		auto it = column.find_last_of('.');
		if (it != string::npos && it < (column.length()-1))
		{
			string afterDot = column.substr(it+1);
			boost::trim(afterDot);
			boost::to_lower(afterDot);
			if (afterDot == "length")
			{
				column = column.substr(0,it);;
				lengthOf = true;
			}
		}
	}
}

const char* CustomDataSource::WhereCond::OperandToStr( Operand op )
{
	if (op == OP_LT)
		return "<";
	else if (op == OP_GT)
		return ">";
	else if (op == OP_EQU)
		return "=";
	else if (op == OP_NEQU)
		return "<>";
	else if (op == OP_ISNULL)
		return "IS NULL";
	else if (op == OP_ISNOTNULL)
		return "IS NOT NULL";
	else if (op == OP_LIKE)
		return "LIKE";
	else if (op == OP_NLIKE)
		return "NOT LIKE";
	else if (op == OP_RLIKE)
		return "RLIKE";
	else if (op == OP_NRLIKE)
		return "NOT RLIKE";

	return "UNKNOWNOP";
}

CustomDataSource::WhereCond::Operand CustomDataSource::WhereCond::OperandFromStr( std::string str )
{
	boost::trim(str);
	boost::to_upper(str);

	//merge all multiple spaces into one
	{
		string oldStr;
		do
		{
			oldStr = str;
			boost::replace_all(str,"  "," ");
		} while (oldStr != str);
	}	

	if (str == "<")
		return OP_LT;
	else if (str == ">")
		return OP_GT;
	else if (str == "=")
		return OP_EQU;
	else if (str == "<>" || str == "!=")
		return OP_NEQU;
	else if (str == "IS NULL")
		return OP_ISNULL;
	else if (str == "IS NOT NULL")
		return OP_ISNOTNULL;
	else if (str == "LIKE")
		return OP_LIKE;
	else if (str == "NOT LIKE")
		return OP_NLIKE;
	else if (str == "RLIKE" || str == "REGEXP")
		return OP_RLIKE;
	else if (str == "NOT RLIKE" || str == "NOT REGEXP")
		return OP_NRLIKE;

	return OP_COUNT;
}

string CustomDataSource::WhereCond::toString( Database* usedDb ) const
{
	string where = usedDb->sqlTableSim(usedDb->escape(this->column));
	if (lengthOf)
		where = "LENGTH(" + where + ")";

	where += string(" ") + OperandToStr(this->operand);

	if (this->operand != OP_ISNULL && this->operand != OP_ISNOTNULL)
		where += " '" + usedDb->escape(this->constant) + "'";

	return where;
}

CustomDataSource::WhereGlue::WhereGlue( string str ) : op(LOG_COUNT), numRepeat(1)
{
	boost::trim(str);
	boost::to_upper(str);

	if (str == "AND")
		op = LOG_AND;
	else if (str == "OR")
		op = LOG_OR;
	else if (str == "NOT")
		op = LOG_NOT;
	else if (str.length() > 0)
	{
		//remove all whitespace between possible braces
		str.erase(remove_if(str.begin(),str.end(),::isspace), str.end());

		char firstChar = str[0];
		for (auto it=str.cbegin();it!=str.cend();++it)
		{
			if (*it != firstChar)
				return;
		}
		if (firstChar == '(')
			op = LOG_LB;
		else if (firstChar == ')')
			op = LOG_RB;
		else
			return;

		numRepeat = str.length();
	}
}

string CustomDataSource::WhereGlue::toString() const
{
	if (op == LOG_AND)
		return "AND";
	else if (op == LOG_OR)
		return "OR";
	else if (op == LOG_NOT)
		return "NOT";
	else if (op == LOG_LB)
		return string(numRepeat,'(');
	else if (op == LOG_RB)
		return string(numRepeat,')');
	else
		return "UNKNOWNLOG";
}

UInt32 CustomDataSource::getRandomNumber() const
{
	UInt32 rndNum = 0;
	do
	{
		rndNum = _rng.next();
	}
	while (rndNum == 0 ||
		_activeRes.count(rndNum) > 0 ||
		_erroredRes.count(rndNum) > 0 ||
		_pendingRes.count(rndNum) > 0);

	return rndNum;
}

UInt32 CustomDataSource::dataRequest( const string& tableName, const vector<string>& columnNames, 
									 const vector<WhereElem>& where, Int64 limitCount, Int64 limitOffset, bool async )
{
	TableInfo tblInfo(tableName);

	if (std::find(_allowed.begin(),_allowed.end(),tblInfo) == _allowed.end())
		throw DisallowedTableException(tblInfo.toString());

	Database* usedDb = getDB(tblInfo.dbase);

	string query = "SELECT ";
	for (size_t i=0; i<columnNames.size(); i++)
	{
		query += usedDb->sqlTableSim(usedDb->escape(columnNames[i]));
		if (i != columnNames.size()-1)
			query += ", ";
	}
	query += " FROM " + usedDb->sqlTableSim(usedDb->escape(tblInfo.table));
	if (where.size() > 0)
		query += " WHERE ";

	for (size_t i=0; i<where.size(); i++)
	{
		const WhereElem& curr = where[i];
		const WhereGlue* glueVal = boost::get<WhereGlue>(&curr);
		if (glueVal != nullptr)
			query += glueVal->toString();
		else
		{
			const WhereCond* clauseVal = boost::get<WhereCond>(&curr);
			if (clauseVal != nullptr)
				query += clauseVal->toString(usedDb);
		}

		if (i != where.size()-1)
			query += " ";
	}

	if (limitCount >= 0 || limitOffset > 0)
	{
		query += " LIMIT ";
		if (limitCount < 0)
			limitCount = 0;

		if (limitOffset > 0)
			query += boost::lexical_cast<string>(limitOffset)+",";

		query += boost::lexical_cast<string>(limitCount);
	}

	UInt32 uniqId = 0;
	if (!async)
	{
		unique_ptr<QueryResult> res = usedDb->query(query.c_str());
		if (!res)
			throw DataFetchException("SQL Error running query: "+query);

		uniqId = getRandomNumber();
		_activeRes.insert(std::make_pair(uniqId,std::move(res)));
	}
	else
	{
		uniqId = getRandomNumber();
		_pendingRes.insert(uniqId);
		usedDb->asyncQuery([&,uniqId,query](QueryCallback::ResType res)
			{
				_pendingRes.erase(uniqId);

				if (_canceledRes.count(uniqId) > 0)
					_canceledRes.erase(uniqId);
				else if (res)
					_activeRes.insert(std::make_pair(uniqId,std::move(res)));
				else
					_erroredRes.insert(std::make_pair(uniqId,"SQL Error running query: "+query));
			}, query.c_str());
	}
	return uniqId;
}

void CustomDataSource::transferPending()
{
	//the async callback will do this, we just have to give it a chance
	for (auto it=_dbs.begin(); it!=_dbs.end(); ++it)
		it->second->invokeCallbacks();
}

CustomDataSource::RequestState CustomDataSource::getRequestState( UInt32 token )
{
	//check if pending
	{
		auto it = _pendingRes.find(token);
		if (it != _pendingRes.end())
			return REQ_PENDING;
	}
	//check if errored
	{
		auto it = _erroredRes.find(token);
		if (it != _erroredRes.end())
		{
			std::string fetchProblem = std::move(it->second);
			_erroredRes.erase(it);
			throw DataFetchException(std::move(fetchProblem));
		}
	}
	//otherwise it's just a bad/expired id
	return REQ_UNKNOWN;
}

CustomDataSource::RequestState CustomDataSource::requestStatus( UInt32 token, UInt64& numRows, size_t& numFields, vector<string>& fieldNames )
{
	this->transferPending();
	auto it = _activeRes.find(token);
	if (it != _activeRes.end())
	{
		numRows = it->second->numRows();
		numFields = it->second->numFields();
		fieldNames = it->second->fetchFieldNames();
		return REQ_OK;
	}
	else
		return this->getRequestState(token);
}

CustomDataSource::RequestState CustomDataSource::getRowData( UInt32 token, vector<RowFieldData>& outRow )
{
	this->transferPending();
	auto it = _activeRes.find(token);
	if (it != _activeRes.end())
	{
		QueryResult* res = it->second.get();
		bool rowAvailable = res->fetchRow();
		if (!rowAvailable)
			return REQ_NOMOREROWS;

		outRow.resize(res->numFields());
		for (size_t i=0; i<outRow.size(); i++)
		{
			const Field& fld = res->at(i);
			if (fld.isNull())
				outRow[i].reset();
			else
				outRow[i] = fld.getString();
		}

		return REQ_OK;
	}
	else
		return this->getRequestState(token);
}

bool CustomDataSource::closeRequest( UInt32 token )
{
	//check in active, destroy result
	{
		auto it = _activeRes.find(token);
		if (it != _activeRes.end())
		{
			_activeRes.erase(it);
			return true;
		}
	}
	//check in pending, issue cancellation
	{
		auto it = _pendingRes.find(token);
		if (it != _pendingRes.end())
		{
			//issue cancellation
			_canceledRes.insert(token);
			//perform cancellation if possible
			this->transferPending();
			return true;
		}
	}
	//check in errored, clear error
	{
		auto it = _erroredRes.find(token);
		if (it != _erroredRes.end())
		{
			_erroredRes.erase(it);
			return true;
		}
	}
	//unknown token
	return false;
}