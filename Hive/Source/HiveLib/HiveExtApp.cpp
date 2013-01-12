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

#include "HiveExtApp.h"

#include <boost/bind.hpp>
#include <boost/optional.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <boost/date_time/gregorian_calendar.hpp>

void HiveExtApp::setupClock()
{
	namespace pt = boost::posix_time;
	pt::ptime utc = pt::second_clock::universal_time();
	pt::ptime now;

	Poco::AutoPtr<Poco::Util::AbstractConfiguration> timeConf(config().createView("Time"));
	string timeType = timeConf->getString("Type","Local");

	if (boost::iequals(timeType,"Custom"))
	{
		now = utc;

		const char* defOffset = "0";
		string offsetStr = timeConf->getString("Offset",defOffset);
		boost::trim(offsetStr);
		if (offsetStr.length() < 1)
			offsetStr = defOffset;
		
		try
		{
			now += pt::duration_from_string(offsetStr);
		}
		catch(const std::exception&)
		{
			logger().warning("Invalid value for Time.Offset configuration variable (expected int, given: "+offsetStr+")");
		}
	}
	else if (boost::iequals(timeType,"Static"))
	{
		now = pt::second_clock::local_time();
		try
		{
			int hourOfTheDay = timeConf->getInt("Hour");
			now -= pt::time_duration(now.time_of_day().hours(),0,0);
			now += pt::time_duration(hourOfTheDay,0,0);
		}
		//do not change hour of the day if bad or missing value in config
		catch(const Poco::NotFoundException&) {}
		catch(const Poco::SyntaxException&) 
		{
			string hourStr = timeConf->getString("Hour","");
			boost::trim(hourStr);
			if (hourStr.length() > 0)
				logger().warning("Invalid value for Time.Hour configuration variable (expected int, given: "+hourStr+")");
		}

		//change the date
		{
			string dateStr = timeConf->getString("Date","");
			boost::trim(dateStr);
			if (dateStr.length() > 0) //only if non-empty value
			{
				namespace gr = boost::gregorian;
				try
				{
					gr::date newDate = gr::from_uk_string(dateStr);
					now = pt::ptime(newDate, now.time_of_day());
				}
				catch(const std::exception&)
				{
					logger().warning("Invalid value for Time.Date configuration variable (expected date, given: "+dateStr+")");
				}
			}
		}
	}
	else
		now = pt::second_clock::local_time();

	_timeOffset = now - utc;
}

#include "Version.h"

int HiveExtApp::main( const std::vector<std::string>& args )
{
	logger().information("HiveExt " + GIT_VERSION.substr(0,12));
	setupClock();

	if (!this->initialiseService())
	{
		logger().close();
		return EXIT_IOERR;
	}

	return EXIT_OK;
}

HiveExtApp::HiveExtApp(string suffixDir) : AppServer("HiveExt",suffixDir), _serverId(-1)
{
	//custom data retrieval
	handlers[500] = boost::bind(&HiveExtApp::changeTableAccess,this,_1);	//mechanism for setting up custom table permissions
	handlers[501] = boost::bind(&HiveExtApp::dataRequest,this,_1,false);	//sync load init and wait
	handlers[502] = boost::bind(&HiveExtApp::dataRequest,this,_1,true);		//async load init
	handlers[503] = boost::bind(&HiveExtApp::dataStatus,this,_1);			//retrieve request status and info
	handlers[504] = boost::bind(&HiveExtApp::dataFetchRow,this,_1);			//fetch row from completed query
	handlers[505] = boost::bind(&HiveExtApp::dataClose,this,_1);			//destroy any trace of request
	//server and object stuff
	handlers[302] = boost::bind(&HiveExtApp::streamObjects,this,_1);		//Returns object count, superKey first time, rows after that
	handlers[303] = boost::bind(&HiveExtApp::objectInventory,this,_1,false);
	handlers[304] = boost::bind(&HiveExtApp::objectDelete,this,_1,false);
	handlers[305] = boost::bind(&HiveExtApp::vehicleMoved,this,_1);
	handlers[306] = boost::bind(&HiveExtApp::vehicleDamaged,this,_1);
	handlers[307] = boost::bind(&HiveExtApp::getDateTime,this,_1);
	handlers[308] = boost::bind(&HiveExtApp::objectPublish,this,_1);
	handlers[309] = boost::bind(&HiveExtApp::objectInventory,this,_1,true);
	handlers[310] = boost::bind(&HiveExtApp::objectDelete,this,_1,true);
	handlers[399] = boost::bind(&HiveExtApp::serverShutdown,this,_1);		//Shut down the hiveExt instance
	//player/character loads
	handlers[101] = boost::bind(&HiveExtApp::loadPlayer,this,_1);
	handlers[102] = boost::bind(&HiveExtApp::loadCharacterDetails,this,_1);
	handlers[103] = boost::bind(&HiveExtApp::recordCharacterLogin,this,_1);
	//character updates
	handlers[201] = boost::bind(&HiveExtApp::playerUpdate,this,_1);
	handlers[202] = boost::bind(&HiveExtApp::playerDeath,this,_1);
	handlers[203] = boost::bind(&HiveExtApp::playerInit,this,_1);
}

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;
using boost::bad_lexical_cast;

void HiveExtApp::callExtension( const char* function, char* output, size_t outputSize )
{
	Sqf::Parameters params;
	try
	{
		params = lexical_cast<Sqf::Parameters>(function);	
	}
	catch(bad_lexical_cast)
	{
		logger().error("Cannot parse function: " + string(function));
		return;
	}

	int funcNum = -1;
	try
	{
		string childIdent = boost::get<string>(params.at(0));
		if (childIdent != "CHILD")
			throw std::runtime_error("First element in parameters must be CHILD");

		params.erase(params.begin());
		funcNum = boost::get<int>(params.at(0));
		params.erase(params.begin());
	}
	catch (...)
	{
		logger().error("Invalid function format: " + string(function));
		return;
	}

	if (handlers.count(funcNum) < 1)
	{
		logger().error("Invalid method id: " + lexical_cast<string>(funcNum));
		return;
	}

	if (logger().debug())
		logger().debug("Original params: |" + string(function) + "|");

	logger().information("Method: " + lexical_cast<string>(funcNum) + " Params: " + lexical_cast<string>(params));
	HandlerFunc handler = handlers[funcNum];
	Sqf::Value res;
	boost::optional<ServerShutdownException> shutdownExc;
	try
	{
		res = handler(params);
	}
	catch (const ServerShutdownException& e)
	{
		if (!e.keyMatches(_initKey))
		{
			logger().error("Actually not shutting down");
			return;
		}

		shutdownExc = e;
		res = e.getReturnValue();
	}
	catch (...)
	{
		logger().error("Error executing |" + string(function) + "|");
		return;
	}		

	string serializedRes = lexical_cast<string>(res);
	logger().information("Result: " + serializedRes);

	if (serializedRes.length() >= outputSize)
		logger().error("Output size too big ("+lexical_cast<string>(serializedRes.length())+") for request : " + string(function));
	else
		strncpy_s(output,outputSize,serializedRes.c_str(),outputSize-1);

	if (shutdownExc.is_initialized())
		throw *shutdownExc;
}

namespace
{
	Sqf::Parameters ReturnStatus(std::string status, Sqf::Parameters rest)
	{
		Sqf::Parameters outRet;
		outRet.push_back(std::move(status));
		for (size_t i=0; i<rest.size(); i++)
			outRet.push_back(std::move(rest[i]));

		return outRet;
	}
	template<typename T>
	Sqf::Parameters ReturnStatus(std::string status, T other)
	{
		Sqf::Parameters rest; rest.push_back(std::move(other));
		return ReturnStatus(std::move(status),std::move(rest));
	}
	Sqf::Parameters ReturnStatus(std::string status)
	{
		return ReturnStatus(std::move(status),Sqf::Parameters());
	}

	Sqf::Parameters ReturnBooleanStatus(bool isGood, string errorMsg = "")
	{
		string retStatus = "PASS";
		if (!isGood)
			retStatus = "ERROR";

		if (errorMsg.length() < 1)
			return ReturnStatus(std::move(retStatus));
		else
			return ReturnStatus(std::move(retStatus),std::move(errorMsg));
	}
};

Sqf::Value HiveExtApp::getDateTime( Sqf::Parameters params )
{
	namespace pt=boost::posix_time;
	pt::ptime now = pt::second_clock::universal_time() + _timeOffset;

	Sqf::Parameters retVal;
	retVal.push_back(string("PASS"));
	{
		Sqf::Parameters dateTime;
		dateTime.push_back(static_cast<int>(now.date().year()));
		dateTime.push_back(static_cast<int>(now.date().month()));
		dateTime.push_back(static_cast<int>(now.date().day()));
		dateTime.push_back(static_cast<int>(now.time_of_day().hours()));
		dateTime.push_back(static_cast<int>(now.time_of_day().minutes()));
		retVal.push_back(dateTime);
	}
	return retVal;
}

#include <Poco/HexBinaryEncoder.h>
#include <Poco/HexBinaryDecoder.h>

#include "DataSource/ObjDataSource.h"
#include <Poco/RandomStream.h>

Sqf::Value HiveExtApp::streamObjects( Sqf::Parameters params )
{
	if (_srvObjects.empty())
	{
		if (_initKey.length() < 1)
		{
			int serverId = boost::get<int>(params.at(0));
			setServerId(serverId);

			_objData->populateObjects(getServerId(), _srvObjects);
			//set up initKey
			{
				boost::array<UInt8,16> keyData;
				Poco::RandomInputStream().read((char*)keyData.c_array(),keyData.size());
				std::ostringstream ostr;
				Poco::HexBinaryEncoder enc(ostr);
				enc.rdbuf()->setLineLength(0);
				enc.write((const char*)keyData.data(),keyData.size());
				enc.close();
				_initKey = ostr.str();
			}

			Sqf::Parameters retVal;
			retVal.push_back(string("ObjectStreamStart"));
			retVal.push_back(static_cast<int>(_srvObjects.size()));
			retVal.push_back(_initKey);
			return retVal;
		}
		else
		{
			Sqf::Parameters retVal;
			retVal.push_back(string("ERROR"));
			retVal.push_back(string("Instance already initialized"));
			return retVal;
		}
	}
	else
	{
		Sqf::Parameters retVal = _srvObjects.front();
		_srvObjects.pop();

		return retVal;
	}
}

Sqf::Value HiveExtApp::objectInventory( Sqf::Parameters params, bool byUID /*= false*/ )
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));
	Sqf::Value inventory = boost::get<Sqf::Parameters>(params.at(1));

	if (objectIdent != 0) //all the vehicles have objectUID = 0, so it would be bad to update those
		return ReturnBooleanStatus(_objData->updateObjectInventory(getServerId(),objectIdent,byUID,inventory));

	return ReturnBooleanStatus(true);
}

Sqf::Value HiveExtApp::objectDelete( Sqf::Parameters params, bool byUID /*= false*/ )
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));

	if (objectIdent != 0) //all the vehicles have objectUID = 0, so it would be bad to delete those
		return ReturnBooleanStatus(_objData->deleteObject(getServerId(),objectIdent,byUID));

	return ReturnBooleanStatus(true);
}

Sqf::Value HiveExtApp::vehicleMoved( Sqf::Parameters params )
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));
	Sqf::Value worldspace = boost::get<Sqf::Parameters>(params.at(1));
	double fuel = Sqf::GetDouble(params.at(2));

	if (objectIdent > 0) //sometimes script sends this with object id 0, which is bad
		return ReturnBooleanStatus(_objData->updateVehicleMovement(getServerId(),objectIdent,worldspace,fuel));

	return ReturnBooleanStatus(true);
}

Sqf::Value HiveExtApp::vehicleDamaged( Sqf::Parameters params )
{
	Int64 objectIdent = Sqf::GetBigInt(params.at(0));
	Sqf::Value hitPoints = boost::get<Sqf::Parameters>(params.at(1));
	double damage = Sqf::GetDouble(params.at(2));

	if (objectIdent > 0) //sometimes script sends this with object id 0, which is bad
		return ReturnBooleanStatus(_objData->updateVehicleStatus(getServerId(),objectIdent,hitPoints,damage));

	return ReturnBooleanStatus(true);
}

Sqf::Value HiveExtApp::objectPublish( Sqf::Parameters params )
{
	string className = boost::get<string>(params.at(1));
	double damage = Sqf::GetDouble(params.at(2));
	int characterId = Sqf::GetIntAny(params.at(3));
	Sqf::Value worldSpace = boost::get<Sqf::Parameters>(params.at(4));
	Sqf::Value inventory = boost::get<Sqf::Parameters>(params.at(5));
	Sqf::Value hitPoints = boost::get<Sqf::Parameters>(params.at(6));
	double fuel = Sqf::GetDouble(params.at(7));
	Int64 uniqueId = Sqf::GetBigInt(params.at(8));

	return ReturnBooleanStatus(_objData->createObject(getServerId(),className,damage,characterId,worldSpace,inventory,hitPoints,fuel,uniqueId));
}

#include "DataSource/CharDataSource.h"

Sqf::Value HiveExtApp::loadPlayer( Sqf::Parameters params )
{
	string playerId = Sqf::GetStringAny(params.at(0));
	string playerName = Sqf::GetStringAny(params.at(2));

	return _charData->fetchCharacterInitial(playerId,getServerId(),playerName);
}

Sqf::Value HiveExtApp::loadCharacterDetails( Sqf::Parameters params )
{
	int characterId = Sqf::GetIntAny(params.at(0));
	
	return _charData->fetchCharacterDetails(characterId);
}

Sqf::Value HiveExtApp::recordCharacterLogin( Sqf::Parameters params )
{
	string playerId = Sqf::GetStringAny(params.at(0));
	int characterId = Sqf::GetIntAny(params.at(1));
	int action = Sqf::GetIntAny(params.at(2));

	return ReturnBooleanStatus(_charData->recordLogin(playerId,characterId,action));
}

Sqf::Value HiveExtApp::playerUpdate( Sqf::Parameters params )
{
	int characterId = Sqf::GetIntAny(params.at(0));
	CharDataSource::FieldsType fields;

	try
	{
		if (!Sqf::IsNull(params.at(1)))
		{
			Sqf::Parameters worldSpaceArr = boost::get<Sqf::Parameters>(params.at(1));
			if (worldSpaceArr.size() > 0)
			{
				Sqf::Value worldSpace = worldSpaceArr;
				fields["Worldspace"] = worldSpace;
			}
		}
		if (!Sqf::IsNull(params.at(2)))
		{
			Sqf::Parameters inventoryArr = boost::get<Sqf::Parameters>(params.at(2));
			if (inventoryArr.size() > 0)
			{
				Sqf::Value inventory = inventoryArr;
				fields["Inventory"] = inventory;
			}
		}
		if (!Sqf::IsNull(params.at(3)))
		{
			Sqf::Parameters backpackArr = boost::get<Sqf::Parameters>(params.at(3));
			if (backpackArr.size() > 0)
			{
				Sqf::Value backpack = backpackArr;
				fields["Backpack"] = backpack;
			}
		}
		if (!Sqf::IsNull(params.at(4)))
		{
			Sqf::Parameters medicalArr = boost::get<Sqf::Parameters>(params.at(4));
			if (medicalArr.size() > 0)
			{
				for (size_t i=0;i<medicalArr.size();i++)
				{
					if (Sqf::IsAny(medicalArr[i]))
					{
						logger().warning("update.medical["+lexical_cast<string>(i)+"] changed from any to []");
						medicalArr[i] = Sqf::Parameters();
					}
				}
				Sqf::Value medical = medicalArr;
				fields["Medical"] = medical;
			}
		}
		if (!Sqf::IsNull(params.at(5)))
		{
			bool justAte = boost::get<bool>(params.at(5));
			if (justAte) fields["JustAte"] = true;
		}
		if (!Sqf::IsNull(params.at(6)))
		{
			bool justDrank = boost::get<bool>(params.at(6));
			if (justDrank) fields["JustDrank"] = true;
		}
		if (!Sqf::IsNull(params.at(7)))
		{
			int moreKillsZ = boost::get<int>(params.at(7));
			if (moreKillsZ > 0) fields["KillsZ"] = moreKillsZ;
		}
		if (!Sqf::IsNull(params.at(8)))
		{
			int moreKillsH = boost::get<int>(params.at(8));
			if (moreKillsH > 0) fields["HeadshotsZ"] = moreKillsH;
		}
		if (!Sqf::IsNull(params.at(9)))
		{
			int distanceWalked = static_cast<int>(Sqf::GetDouble(params.at(9)));
			if (distanceWalked > 0) fields["DistanceFoot"] = distanceWalked;
		}
		if (!Sqf::IsNull(params.at(10)))
		{
			int durationLived = static_cast<int>(Sqf::GetDouble(params.at(10)));
			if (durationLived > 0) fields["Duration"] = durationLived;
		}
		if (!Sqf::IsNull(params.at(11)))
		{
			Sqf::Parameters currentStateArr = boost::get<Sqf::Parameters>(params.at(11));
			if (currentStateArr.size() > 0)
			{
				Sqf::Value currentState = currentStateArr;
				fields["CurrentState"] = currentState;
			}
		}
		if (!Sqf::IsNull(params.at(12)))
		{
			int moreKillsHuman = boost::get<int>(params.at(12));
			if (moreKillsHuman > 0) fields["KillsH"] = moreKillsHuman;
		}
		if (!Sqf::IsNull(params.at(13)))
		{
			int moreKillsBandit = boost::get<int>(params.at(13));
			if (moreKillsBandit > 0) fields["KillsB"] = moreKillsBandit;
		}
		if (!Sqf::IsNull(params.at(14)))
		{
			string newModel = boost::get<string>(params.at(14));
			fields["Model"] = newModel;
		}
		if (!Sqf::IsNull(params.at(15)))
		{
			int humanityDiff = static_cast<int>(Sqf::GetDouble(params.at(15)));
			if (humanityDiff != 0) fields["Humanity"] = humanityDiff;
		}
	}
	catch (const std::out_of_range&)
	{
		logger().warning("Update of character " + lexical_cast<string>(characterId) + " only had " + lexical_cast<string>(params.size()) + " parameters out of 16");
	}

	if (fields.size() > 0)
		return ReturnBooleanStatus(_charData->updateCharacter(characterId,fields));

	return ReturnBooleanStatus(true);
}

Sqf::Value HiveExtApp::playerInit( Sqf::Parameters params )
{
	int characterId = Sqf::GetIntAny(params.at(0));
	Sqf::Value inventory = boost::get<Sqf::Parameters>(params.at(1));
	Sqf::Value backpack = boost::get<Sqf::Parameters>(params.at(2));

	return ReturnBooleanStatus(_charData->initCharacter(characterId,inventory,backpack));
}

Sqf::Value HiveExtApp::playerDeath( Sqf::Parameters params )
{
	int characterId = Sqf::GetIntAny(params.at(0));
	int duration = static_cast<int>(Sqf::GetDouble(params.at(1)));
	
	return ReturnBooleanStatus(_charData->killCharacter(characterId,duration));
}

namespace
{
	class WhereVisitor : public boost::static_visitor<CustomDataSource::WhereElem>
	{
	public:
		CustomDataSource::WhereElem operator()(const std::string& opStr) const 
		{
			auto glue = CustomDataSource::WhereGlue(opStr);
			if (!glue.isValid())
				throw std::string("Logical operator unknown: '"+opStr+"'");
			return glue;
		}
		CustomDataSource::WhereElem operator()(const Sqf::Parameters& condArr) const
		{
			const char* currElem;
			try
			{
				currElem = "COLUMN";
				const auto& columnStr = boost::get<string>(condArr.at(0));

				currElem = "OP";
				const auto& opStr = boost::get<string>(condArr.at(1));
				auto opReal = CustomDataSource::WhereCond::OperandFromStr(opStr);
				if (opReal >= CustomDataSource::WhereCond::OP_COUNT)
					throw std::string("Condition has unknown OP '" + opStr + "'");

				string constantStr;
				if (opReal != CustomDataSource::WhereCond::OP_ISNULL &&
					opReal != CustomDataSource::WhereCond::OP_ISNOTNULL)
				{
					currElem = "CONSTANT";
					constantStr = boost::get<string>(condArr.at(2));
				}

				auto cond = CustomDataSource::WhereCond(columnStr,opReal,constantStr);
				if (!cond.isValid())
					throw std::string("Condition COLUMN is empty");

				return cond;
			}
			catch(const boost::bad_get&)
			{
				throw std::string("Condition " + string(currElem) + " not a string");
			}
			catch(const std::out_of_range&)
			{
				throw std::string("Condition doesn't have " + string(currElem) + " element");
			}
		}
		template <typename T> CustomDataSource::WhereElem operator()(T) const
		{
			throw boost::bad_get();
		}
	};

	std::string	TokenToHex(UInt32 token)
	{
		std::ostringstream ostr;
		Poco::HexBinaryEncoder enc(ostr);
		enc.rdbuf()->setLineLength(0);
		enc.write((const char*)&token,sizeof(token));
		enc.close();
		return ostr.str();
	}

	UInt32 HexToToken(std::string strData)
	{
		strData.erase(remove_if(strData.begin(),strData.end(),::isspace), strData.end());
		if (strData.length() != sizeof(UInt32)*2)
			throw boost::bad_lexical_cast();

		UInt32 token = 0;
		std::istringstream istr(strData);
		try
		{
			Poco::HexBinaryDecoder dec(istr);
			dec.read((char*)&token,sizeof(token));
		}
		catch(const Poco::DataFormatException&)
		{
			throw boost::bad_lexical_cast();
		}

		return token;
	}

	UInt32 FetchToken(const Sqf::Parameters& params)
	{
		try
		{
			return HexToToken(Sqf::GetStringAny(params.at(0)));
		}
		catch(const boost::bad_lexical_cast&)
		{
			//invalid characters in string
			return 0;
		}
		catch(const boost::bad_get&)
		{
			//not a string
			return 0;
		}
		catch(const std::out_of_range&)
		{
			//doesn't even exist
			return 0;
		}
	}
};

//CHILD:501:DbName.TableName:["ColumnName1","ColumnName2"]:["NOT",["ColumnNameX","<","Constant"]],"AND",["SomeOtherColumn","RLIKE","[0-9]"]]:[0,50]:
//CHILD:FUNC:TBLNAME:COLUMNSARR:WHEREARR:LIMITS

//If you use function number 501 the request is synchronous (and query errors are returned immediately)
//otherwise, function number 502 is asynchronous, which means the data might be in WAIT state for a while

//DbName in TBLNAME is either Character or Object
//The requested Table must be previously-enabled for custom data queries through HiveExt.ini

//COLUMNSARR is an array of column names to fetch
//alternatively, COLUMNSARR can be a single string called COUNT to get the row count ONLY

//WHEREARR is an array, whose elements can either be:
//1. a single string, which denotes the boolean-link operator to apply
//in this case, it can be "AND", "OR", "NOT", or any number of "(" or ")"
//2. an array of 3 elements [COLUMN,OP,CONSTANT], all 3 elements should be strings
//COLUMN is the column on which comparison OP will be applied to
//you can append .length to COLUMN to use it's length instead of it's value
//OP can be "<", ">", "=", "<>", "IS NULL", "IS NOT NULL", "LIKE", "NOT LIKE", "RLIKE", "NOT RLIKE"
//CONSTANT is a literal value with which to perform the comparison
//in the LIKE/RLIKE case, it's either a LIKE formatting string, or a REGEXP formatted string

//LIMITS is either an array of two numbers, [OFFSET, COUNT]
//or a single number COUNT
//this corresponds to the SQL versions of LIMIT COUNT or LIMIT OFFSET,COUNT
//this parameter is optional, you can omit it by just not having that :[*] at the end

//The return value is either ["PASS",UNIQID] where UNIQID represents the string token that you can later use to retrieve results
//or ["ERROR",ERRORDESCR] where ERRORDESCR is a description of the error that happened
Sqf::Value HiveExtApp::dataRequest( Sqf::Parameters params, bool async )
{
	auto retErr = [](string errMsg) -> Sqf::Value
	{
		vector<Sqf::Value> errRtn; errRtn.push_back(string("ERROR")); errRtn.push_back(std::move(errMsg));
		return errRtn;
	};

	auto tableName = boost::get<string>(params.at(0));
	vector<string> fields;
	{
		int currIdx = -1;
		try
		{
			const auto& sqfFields = boost::get<Sqf::Parameters>(params.at(1));
			fields.reserve(sqfFields.size());
			for (size_t i=0; i<sqfFields.size(); i++)
			{
				currIdx++;
				fields.push_back(boost::get<string>(sqfFields[i]));
			}
		}
		catch(const boost::bad_get&)
		{
			string errorMsg;
			if (currIdx < 0)
				errorMsg = "FIELDS not an array";
			else
				errorMsg = "FIELDS[" + boost::lexical_cast<string>(currIdx) + "] not a string";
		}
	}
	vector<CustomDataSource::WhereElem> where;
	{
		const auto& whereSqfArr = boost::get<Sqf::Parameters>(params.at(2));
		for (size_t i=0; i<whereSqfArr.size(); i++)
		{
			try
			{
				where.push_back(boost::apply_visitor(WhereVisitor(),whereSqfArr[i]));
			}
			catch (const boost::bad_get&)
			{
				string errorMsg = "WHERE[" + boost::lexical_cast<string>(i) + "] not a string or array";
				return retErr(errorMsg);
			}
			catch(const std::string& e)
			{
				string errorMsg = "WHERE[" + boost::lexical_cast<string>(i) + "] " + e;
				return retErr(errorMsg);
			}
		}
	}

	Int64 limitCount = -1;
	Int64 limitOffset = 0;

	if (params.size() >= 4)
	{
		try
		{
			limitCount = Sqf::GetBigInt(params[3]);
		}
		catch (const boost::bad_get&)
		{
			try
			{
				const auto& limitArr = boost::get<Sqf::Parameters>(params[3]);
				if (limitArr.size() < 2)
					throw boost::bad_get();

				limitOffset = Sqf::GetBigInt(limitArr[0]);
				limitCount = Sqf::GetBigInt(limitArr[1]);
			}
			catch (const boost::bad_get&)
			{
				string errorMsg = "LIMIT in invalid format: '"+boost::lexical_cast<string>(params[3])+"'";
				return retErr(errorMsg);
			}
		}
	}

	try
	{
		UInt32 token = _customData->dataRequest(tableName,fields,where,limitCount,limitOffset,async);
		vector<Sqf::Value> goodRtn;
		goodRtn.push_back(string("PASS"));
		goodRtn.push_back(TokenToHex(token));
		return goodRtn;
	}
	catch(const CustomDataSource::DataException& e)
	{
		return retErr(e.toString());
	}
}

namespace
{
	Sqf::Value ReturnBadToken(bool reallyBad = true)
	{
		return ReturnStatus("UNKID",reallyBad);
	}

	Sqf::Value ReturnError(std::string errMsg)
	{
		return ReturnBooleanStatus(false,std::move(errMsg));
	}

	Sqf::Value HandleRequestState(CustomDataSource::RequestState state)
	{
		if (state == CustomDataSource::REQ_PENDING)
			return ReturnStatus("WAIT");
		else if (state == CustomDataSource::REQ_NOMOREROWS)
			return ReturnStatus("NOMORE");
		else if (state == CustomDataSource::REQ_UNKNOWN)
			return ReturnBadToken(false);
		else
			return ReturnError("Unknown status");
	}
};

//CHILD:503:UNIQID:
//UNIQID is the string you received with a call to 501/502
//the return value is either
//["PASS",numRows,numFields,[field1,field2]]
//["WAIT"]
//["ERROR",ERRORDESCR]
//["UNKID",isInvalidId]
//"PASS" return code gives you information about the query, 
//like total number of rows, number of fields, and the field names in an array
//"WAIT" = the asynchronous operation didn't complete yet
//"ERROR" = the asynchronous operation failed, error info is in ERRORDESCR
//if you get this result, the UNIQID will not be usable anymore (not even for status)
//"UNKID" = unknown UNIQID specified, or it has been cleared (by fetching ERROR status or last row)
//additiionally, if isInvalidId is set to true, then the UNIQID is malformed/missing and would never have worked
Sqf::Value HiveExtApp::dataStatus( Sqf::Parameters params )
{
	UInt32 token = FetchToken(params);
	if (!token)
		return ReturnBadToken();
	
	try
	{
		UInt64 numRows = 0;
		size_t numCols = 0;
		vector<string> fields;
		auto reqStatus = _customData->requestStatus(token,numRows,numCols,fields);
		if (reqStatus == CustomDataSource::REQ_OK)
		{
			Sqf::Parameters retVal;
			retVal.push_back(static_cast<Int64>(numRows));
			retVal.push_back(static_cast<Int64>(numCols));
			{
				Sqf::Parameters realFields;
				for (auto it=fields.begin(); it!=fields.end(); ++it)
					realFields.push_back(Sqf::Value(std::move(*it)));

				retVal.push_back(std::move(realFields));
			}
			
			return ReturnStatus("PASS",std::move(retVal));
		}
		return HandleRequestState(reqStatus);
	}
	catch(const CustomDataSource::DataException& e)
	{
		return ReturnError(e.toString());
	}
}

//CHILD:504:UNIQID:
//see documentation for 503 for everything except when the return code is PASS or NOMORE:
//["PASS",["fieldVal1","fieldVal2",false,"fieldVal3"]]
//the second element of the return array is the array of field values
//each field value will just be a string, EXCEPT if the field IS NULL
//then the field value will be a boolean false
//["NOMORE"]
//indicates that the result set rows have been exhausted
//no actual field values are returned, just a marker to let you know that you should stop
//and close the request
Sqf::Value HiveExtApp::dataFetchRow( Sqf::Parameters params )
{
	UInt32 token = FetchToken(params);
	if (!token)
		return ReturnBadToken();

	try
	{
		vector<CustomDataSource::RowFieldData> values;
		auto reqStatus = _customData->getRowData(token,values);
		if (reqStatus == CustomDataSource::REQ_OK)
		{
			Sqf::Parameters retVal;
			{
				Sqf::Parameters sqfVals;
				for (size_t i=0; i<values.size(); i++)
				{
					if (!values[i].is_initialized())
						sqfVals.push_back(false);
					else
						sqfVals.push_back(std::move(*values[i]));
				}
				retVal.push_back(std::move(sqfVals));
			}

			return ReturnStatus("PASS",std::move(retVal));
		}
		return HandleRequestState(reqStatus);
	}
	catch(const CustomDataSource::DataException& e)
	{
		return ReturnError(e.toString());
	}
}

//CHILD:504:UNIQID:
//closes a retrieved request or cancels a pending one
//returns PASS if it was closed/cancelled
//returns UNKID if the UNIQID was already closed/bad
Sqf::Value HiveExtApp::dataClose( Sqf::Parameters params )
{
	UInt32 token = FetchToken(params);
	if (!token)
		return ReturnBadToken();

	bool closed = _customData->closeRequest(token);
	if (closed)
		return ReturnBooleanStatus(true);
	else
		return ReturnBadToken(false);
}

namespace
{
	struct TableVisitor : public boost::static_visitor<void>
	{
		TableVisitor() : lastIdx(-1) {}

		void operator()(std::string tblName) 
		{
			boost::trim(tblName);
			if (tblName.length() > 0)
				collection.push_back(std::move(tblName));
		}
		void operator()(const Sqf::Parameters& tblNames)
		{
			for (size_t i=0; i<tblNames.size(); i++)
			{
				lastIdx = i;
				collection.push_back(boost::get<string>(tblNames[i]));
			}
		}
		template<typename T>
		void operator()(T) const
		{
			throw boost::bad_get();
		}

		int lastIdx;
		vector<string> collection;
	};
};

//CHILD:500:SUPERKEY:ALLOWTABLES:REMOVEALLOWTABLES:
//ALLOWTABLES and REMOVEALLOWTABLES can either be a string or array of strings
//each represents a table in format DbType.TableName to be allowed/removed from allow list
//having them both blank or missing will give you a result of all the currently allowed tables
//in format ["PASS",["Character.Table1","Object.Table2"]]
//an invalid string for any of the table names would make the whole method fail with
//["ERROR",ERRORDESCR]
//otherwise, the return format is
//["PASS",DUPLICATEALLOWED,REMOVEMISSING]
//where DUPLICATEALLOWED is an array of tables which you have now allowed, but were allowed anyway
//and REMOVEMISSING is an array of tables which you wanted to remove from the allow list, but they weren't there
Sqf::Value HiveExtApp::changeTableAccess( Sqf::Parameters params )
{
	//check key
	{
		string theirKey = boost::get<string>(params.at(0));
		if (!_initKey.length() || _initKey != theirKey)
			return ReturnBooleanStatus(false,"Invalid key");
	}

	TableVisitor visitor;
	vector<string> allowTables;
	vector<string> removeTables;
	const char* currThing;
	try
	{
		if (params.size() >= 2)
		{
			currThing = "ALLOWTABLES";
			visitor = TableVisitor();
			boost::apply_visitor(visitor,params[1]);
			allowTables = visitor.collection;
		}
		if (params.size() >= 3)
		{
			currThing = "REMOVEALLOWTABLES";
			visitor = TableVisitor();
			boost::apply_visitor(visitor,params[2]);
			removeTables = visitor.collection;
		}
	}
	catch(const boost::bad_get&)
	{
		string errorMsg = currThing;
		if (visitor.lastIdx >= 0)
			errorMsg += "[" + boost::lexical_cast<string>(visitor.lastIdx)+"]";
		
		errorMsg += " not a string";

		return ReturnBooleanStatus(false,errorMsg);
	}

	//if neither specified, return current tables
	if (allowTables.size() < 1 && removeTables.size() < 1)
	{
		Sqf::Value retVal;
		{
			vector<string> ourTables = _customData->getAllowedTables();
			Sqf::Parameters tablesSqf(ourTables.size());
			for (size_t i=0; i<tablesSqf.size(); i++)
				tablesSqf[i] = std::move(ourTables[i]);

			retVal = std::move(tablesSqf);
		}
		return ReturnStatus("PASS",retVal);
	}

	//otherwise check input table names
	{
		size_t i;
		try
		{
			currThing = "ALLOWTABLES";
			for (i=0; i<allowTables.size(); i++)
				CustomDataSource::VerifyTable(allowTables[i]);
			currThing = "REMOVEALLOWTABLES";
			for (i=0; i<removeTables.size(); i++)
				CustomDataSource::VerifyTable(removeTables[i]);
		}
		catch(const CustomDataSource::DataException& e)
		{
			string errorMsg = currThing;
			errorMsg += "[" + boost::lexical_cast<string>(i) + "]: " + e.toString();

			return ReturnBooleanStatus(false,errorMsg);
		}
	}

	//then apply the changes
	Sqf::Parameters failedAdd, failedRem;
	try
	{
		for (auto it=allowTables.begin(); it!=allowTables.end(); ++it)
		{
			if (!_customData->allowTable(*it))
				failedAdd.push_back(*it);
		}
		for (auto it=removeTables.begin(); it!=removeTables.end(); ++it)
		{
			if (!_customData->removeAllowedTable(*it))
				failedRem.push_back(*it);
		}
	}
	catch (const CustomDataSource::DataException& e)
	{
		return ReturnBooleanStatus(false,e.toString());
	}

	Sqf::Parameters retVal;
	retVal.push_back(failedAdd);
	retVal.push_back(failedRem);

	return ReturnStatus("PASS",retVal);
}

//CHILD:399:SUPERKEY:
//if the SUPERKEY matches, HiveExt instance will shut down
//and ["PASS"] will be returned
//otherwise, ["ERROR"] will be returned
Sqf::Value HiveExtApp::serverShutdown( Sqf::Parameters params )
{
	string theirKey = boost::get<string>(params.at(0));
	if ((_initKey.length() > 0) && (theirKey == _initKey))
	{
		logger().information("Shutting down HiveExt instance");
		throw ServerShutdownException(theirKey,ReturnBooleanStatus(true));
	}

	return ReturnBooleanStatus(false);
}
