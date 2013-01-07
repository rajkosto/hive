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

#include "DataSource.h"
#include "Shared/Common/Exception.h"
#include <Poco/Random.h>
#include <set>
#include <boost/optional.hpp>

class Database;
class QueryResult;
namespace Poco { namespace Util { class AbstractConfiguration; }; };
class CustomDataSource : public DataSource
{
public:
	CustomDataSource(Poco::Logger& logger, shared_ptr<Database> charDb, shared_ptr<Database> objDb);
	~CustomDataSource();

	struct DataException : public GenericException<std::runtime_error>
	{
		DataException(const char* descr) : GenericException(descr) {}
	};

	struct InvalidTableException : public DataException
	{
		InvalidTableException(string tableName, string descr = "Invalid TableInfo") 
			: DataException("InvalidTableException"), _tableName(std::move(tableName)), _descr(descr) {}
		InvalidTableException(string tableName, string descr,  const char* excName) 
			: DataException(excName), _tableName(std::move(tableName)), _descr(descr) {}
		string toString() const override { return descr() + ": " + tableName(); }
		string tableName() const { return _tableName; }
		string descr() const { return _descr; }
	private:
		string _tableName;
		string _descr;
	};

	static void VerifyTable(string whichOne);
	bool allowTable(string whichOne);
	bool removeAllowedTable(string whichOne);
	vector<string> getAllowedTables() const;

	struct DisallowedTableException : public InvalidTableException
	{
		DisallowedTableException(string tableName, string descr = "TableInfo not allowed") 
			: InvalidTableException(std::move(tableName), descr, "DisallowedTableException") {}
	};

	struct DataFetchException: public DataException
	{
		DataFetchException(string fetchProblem) : DataException("DataFetchException"), _fetchProblem(std::move(fetchProblem)) {}
		string toString() const override { return "Error: " + fetchProblem(); }
		string fetchProblem() const { return _fetchProblem; }
	private:
		string _fetchProblem;
	};

	struct WhereCond
	{
		enum Operand
		{
			OP_LT,
			OP_GT,
			OP_EQU,
			OP_NEQU,
			OP_ISNULL,
			OP_ISNOTNULL,
			OP_LIKE,
			OP_NLIKE,
			OP_RLIKE,
			OP_NRLIKE,
			OP_COUNT
		};	

		WhereCond() : operand(OP_COUNT),lengthOf(false) {}
		WhereCond(string column, Operand operand, string constant);

		static const char* OperandToStr(Operand op);
		static Operand OperandFromStr(std::string str);

		string toString(Database* usedDb) const;
		bool isValid() const { return (operand < OP_COUNT && column.length() > 0); }

		string column;
		bool lengthOf;
		Operand operand;
		string constant;	
	};
	struct WhereGlue
	{
		enum GlueChar
		{
			LOG_AND,
			LOG_OR,
			LOG_NOT,
			LOG_LB,
			LOG_RB,
			LOG_COUNT
		};

		WhereGlue() : op(LOG_COUNT), numRepeat(0) {}
		WhereGlue(string str);
		string toString() const;
		bool isValid() const { return op < LOG_COUNT; }

		GlueChar op;
		size_t numRepeat;
	};
	typedef boost::variant<WhereGlue,WhereCond> WhereElem;
	//can throw any descendant of DataException
	UInt32 dataRequest(const string& tableName, const vector<string>& columnNames, 
		const vector<WhereElem>& where, Int64 limitCount = -1, Int64 limitOffset = -1, bool async = false);

	enum RequestState
	{
		REQ_OK,
		REQ_NOMOREROWS,
		REQ_PENDING,
		REQ_UNKNOWN		
	};
	//any of these below can throw DataFetchException if the async result had an error (except close)
	RequestState requestStatus(UInt32 token, UInt64& numRows, size_t& numFields, vector<string>& fieldNames);

	typedef boost::optional<string> RowFieldData;
	RequestState getRowData(UInt32 token, vector<RowFieldData>& outRow);

	//frees memory of completed requests, cancels pending async requests, clears errors of failed requests
	//throws nothing, returns false if token unknown
	bool closeRequest(UInt32 token);
protected:
	enum DbSource 
	{
		DB_CHAR,
		DB_OBJ,
		DB_UNK
	};
	struct TableInfo
	{
		TableInfo() : dbase(DB_UNK) {}
		TableInfo(std::string inputName);
		TableInfo(DbSource src_, string tblName_) : dbase(src_), table(std::move(tblName_)) {}
		bool operator==(const TableInfo& rhs) const;
		bool operator!=(const TableInfo& rhs) const
		{
			if (*this == rhs)
				return false;
			return true;
		}

		string toString() const
		{
			const char* dbName = "Unknown";
			if (dbase == DB_CHAR)
				dbName = "Character";
			else if (dbase == DB_OBJ)
				dbName = "Object";

			return dbName + ("." + table);
		}
		bool isValid() const { return (dbase < DB_UNK && table.length() > 0); }

		DbSource dbase;
		string table;
	};

	Database* getDB(DbSource src) const 
	{ 
		if (src >= DB_UNK)
			return nullptr;

		auto it = _dbs.find(src);
		poco_assert(it != _dbs.end());
		return it->second.get();
	}

	void transferPending();
	RequestState getRequestState(UInt32 token);
private:
	map<DbSource,shared_ptr<Database>> _dbs;
	vector<TableInfo> _allowed;
	mutable Poco::Random _rng;
	UInt32 getRandomNumber() const;
	//results from all sync and from well-executed async go here
	typedef std::map<UInt32,unique_ptr<QueryResult>> ResultColl;
	ResultColl _activeRes;
	//errored async go here so their status can be retrieved
	typedef std::map<UInt32,string> ErroredResultColl;
	ErroredResultColl _erroredRes;
	//not-yet executed async go here so that status can differentiate WAIT from MISSING
	typedef std::set<UInt32> PendingResultColl;
	PendingResultColl _pendingRes;
	PendingResultColl _canceledRes;
};