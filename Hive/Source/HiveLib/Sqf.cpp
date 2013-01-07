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

#include "Sqf.h"

#include <boost/spirit/include/qi.hpp>
namespace qi=boost::spirit::qi;

namespace 
{
	template <typename Iterator, typename Skipper>
	struct SqfValueParser : qi::grammar<Iterator, Sqf::Value(), Skipper>
	{
		SqfValueParser() : SqfValueParser::base_type(start,"Sqf::Value")
		{
			using qi::lit;
			using qi::lexeme;
			using boost::spirit::ascii::char_;
			using qi::int_;
			using qi::long_long;
			using qi::bool_;

			quoted_string = lexeme['"' >> *(char_ - '"') >> '"'] | lexeme["'" >> *(char_ - "'") >> "'"];
			quoted_string.name("quoted_string");

			start = strict_double |
				(int_ >> !qi::digit) |
				long_long |
				bool_ |
				quoted_string |
				(lit("any") >> qi::attr(static_cast<void*>(nullptr))) |
				(lit("[") >> -(start % ",") >> lit("]"));
		}

		qi::rule<Iterator, string()> quoted_string;
		qi::real_parser< double, qi::strict_real_policies<double> > strict_double;
		qi::rule<Iterator, Sqf::Value(), Skipper> start;
	};

	template <typename Iterator, typename Skipper>
	struct SqfParametersParser : qi::grammar<Iterator, Sqf::Parameters(), Skipper>
	{
		SqfParametersParser() : SqfParametersParser::base_type(start,"Sqf::Parameters")
		{
			using qi::char_;
			using qi::lit;
			using qi::lexeme;
			one_value = (val_parser >> &lit(":")) | (lexeme[*(char_ - ":")] >> &lit(":"));
			one_value.name("one_value");
			start = *(one_value >> ":");
		}

		SqfValueParser<Iterator,Skipper> val_parser;
		qi::rule<Iterator, Sqf::Value(), Skipper> one_value;
		qi::rule<Iterator, Sqf::Parameters(), Skipper> start;
	};
};

namespace
{
	typedef boost::spirit::istream_iterator iter_t; 
}

namespace boost
{
	std::istream& operator >> (std::istream& src, Sqf::Value& out)
	{
		src.unsetf(std::ios::skipws);
		iter_t begin(src);
		iter_t end;
		if (!qi::phrase_parse(begin,end,SqfValueParser<iter_t,qi::space_type>(),qi::space_type(),out))
			src.setstate(std::ios::failbit);

		return src;
	}
};

namespace std
{
	std::istream& operator >> (std::istream& src, Sqf::Parameters& out)
	{
		src.unsetf(std::ios::skipws);
		iter_t begin(src);
		iter_t end;
		if(!qi::phrase_parse(begin,end,SqfParametersParser<iter_t,qi::space_type>(),qi::space_type(),out))
			src.setstate(std::ios::failbit);

		return src;
	}
};


#include <boost/spirit/include/karma.hpp>
namespace karma=boost::spirit::karma;

namespace
{
	template <typename Iterator>
	struct SqfValueGenerator : karma::grammar<Iterator, Sqf::Value()>
	{
		SqfValueGenerator() : SqfValueGenerator::base_type(start,"Sqf::Value")
		{
			using karma::lit;
			using karma::verbatim;
			using karma::int_;
			using karma::long_long;
			using karma::bool_;
			using karma::double_;

			quoted_string = verbatim['"' << karma::string << '"'];
			quoted_string.name("quoted_string");

			complex_array = lit("[") << -(start % ",") << lit("]");
			complex_array.name("complex_array");

			void_pointer = karma::omit[int_] << lit("any");
			void_pointer.name("void_pointer");

			start = double_ | long_long | int_ | bool_ | quoted_string | void_pointer | complex_array;
		}

		karma::rule<Iterator, string()> quoted_string;
		karma::rule<Iterator, vector<Sqf::Value>()> complex_array;
		karma::rule<Iterator, void*()> void_pointer;
		karma::rule<Iterator, Sqf::Value()> start;
	};

	template <typename Iterator>
	struct SqfParametersGenerator : karma::grammar<Iterator, Sqf::Parameters()>
	{
		SqfParametersGenerator() : SqfParametersGenerator::base_type(start,"Sqf::Parameters")
		{
			using karma::lit;
			using karma::verbatim;
			one_value.quoted_string = verbatim[karma::string];
			start = *(one_value << ":");
		}

		SqfValueGenerator<Iterator> one_value;
		karma::rule<Iterator, Sqf::Parameters()> start;
	};
};

namespace boost
{
	std::ostream& operator<<( std::ostream& out, const Sqf::Value& val )
	{
		out << karma::format(SqfValueGenerator<karma::ostream_iterator<char>>(), val);
		return out;
	}
};

namespace std
{
	std::ostream& operator<<( std::ostream& out, const Sqf::Parameters& params )
	{
		out << karma::format(SqfParametersGenerator<karma::ostream_iterator<char>>(), params);
		return out;
	}
};

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace
{
	class NullVisitor : public boost::static_visitor<bool>
	{
	public:
		bool operator()(const std::string& str) const
		{
			if (str.length() < 1)
				return true;

			return false;
		}
		template<typename T> bool operator()(const T& other) const { return false; }
	};

	class AnyVisitor : public boost::static_visitor<bool>
	{
	public:
		typedef void* void_ptr;
		bool operator()(const void_ptr& ptr) const 
		{ 
			return (ptr == nullptr); 
		}
		template<typename T> bool operator()(const T& other) const { return false; }
	};

	class DecimalVisitor : public boost::static_visitor<double>
	{
	public:
		double operator()(double decVal) const { return decVal; }
		double operator()(float decVal) const { return static_cast<double>(decVal); }
		double operator()(int intVal) const { return static_cast<double>(intVal); }
		template<typename T> double operator()(const T& other) const { throw boost::bad_get(); }
	};

	class IntAnyVisitor : public boost::static_visitor<int>
	{
	public:
		int operator()(int normalInt) const { return normalInt; }
		int operator()(const string& strInt) const
		{
			int parsed = -1;
			try
			{
				parsed = boost::lexical_cast<int>(strInt);
			}
			catch (boost::bad_lexical_cast)
			{
				throw boost::bad_get();
			}
			return parsed;
		}
		template<typename T> int operator()(const T& other) const { throw boost::bad_get(); }
	};

	class BigIntVisitor : public boost::static_visitor<Int64>
	{
	public:
		Int64 operator()(Int64 bigInt) const { return bigInt; }
		Int64 operator()(int smallInt) const { return static_cast<Int64>(smallInt); }
		//if a value has a lot of trailing zeroes, it will turn it into exponent notation
		Int64 operator()(double dblInt) const
		{
			Int64 returnVal = static_cast<Int64>(dblInt);
			if (returnVal != dblInt)
				throw boost::bad_get();

			return returnVal;
		}
		Int64 operator()(const string& strInt) const
		{
			Int64 parsed = -1;
			try
			{
				parsed = boost::lexical_cast<Int64>(strInt);
			}
			catch (boost::bad_lexical_cast)
			{
				throw boost::bad_get();
			}
			return parsed;
		}
		template<typename T> Int64 operator()(const T& other) const	{ throw boost::bad_get(); }
	};

	class StringAnyVisitor : public boost::static_visitor<string>
	{
	public:
		string operator()(const string& origStr) const { return origStr; }
		template<typename T> string operator()(const T& other) const { return lexical_cast<string>(other); }
	};

	class BooleanVisitor : public boost::static_visitor<bool>
	{
	public:
		bool operator()(std::string someStr) const 
		{
			boost::trim(someStr);
			if (someStr.length() < 1)
				return false;
			if (boost::iequals(someStr,"false"))
				return false;
			if (boost::iequals(someStr,"true"))
				return true;

			try
			{
				double numeric = boost::lexical_cast<double>(someStr);
				return (*this)(numeric);
			}
			catch (const boost::bad_lexical_cast&)
			{
				return true; //any non-number non-empty string is true
			}
		}
		bool operator()(const Sqf::Parameters& arr) const
		{
			return (arr.size() > 0);
		}
		template<typename T> bool operator()(T other) const { return other != 0; }
	};
};

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

namespace Sqf
{
	bool IsNull(const Value& val)
	{
		return boost::apply_visitor(NullVisitor(),val);
	}

	bool IsAny(const Value& val)
	{
		return boost::apply_visitor(AnyVisitor(),val);
	}

	double GetDouble( const Value& val )
	{
		return boost::apply_visitor(DecimalVisitor(),val);
	}

	int GetIntAny(const Value& val)
	{
		return boost::apply_visitor(IntAnyVisitor(),val);
	}

	Int64 GetBigInt(const Value& val)
	{
		return boost::apply_visitor(BigIntVisitor(),val);
	}

	string GetStringAny(const Value& val)
	{
		return boost::apply_visitor(StringAnyVisitor(),val);
	}

	bool GetBoolAny(const Value& val)
	{
		return boost::apply_visitor(BooleanVisitor(),val);
	}

	void runTest()
	{
		poco_assert(GetBoolAny(Value(true)) == true);
		poco_assert(GetBoolAny(Value(false)) == false);
		poco_assert(GetBoolAny(Value((void*)nullptr)) == false);
		poco_assert(GetBoolAny(Value(string("true"))) == true);
		poco_assert(GetBoolAny(Value(string("false"))) == false);
		poco_assert(GetBoolAny(Value(string("0.0"))) == false);
		poco_assert(GetBoolAny(Value(0.0)) == false);
		poco_assert(GetBoolAny(Value(0)) == false);
		poco_assert(GetBoolAny(Value(1.5)) == true);
		poco_assert(GetBoolAny(Value(string("-1.5"))) == true);
		poco_assert(GetBoolAny(Value(5)) == true);
		poco_assert(GetBoolAny(lexical_cast<Value>(string("[]"))) == false);
		poco_assert(GetBoolAny(lexical_cast<Value>(string("[false]"))) == true);
		poco_assert(GetBoolAny(Value(string(""))) == false);

		vector<string> testSamples;
		testSamples.push_back("5");
		testSamples.push_back("5.0");
		testSamples.push_back("\"hello\"");
		testSamples.push_back("[]");
		testSamples.push_back("[5,\"hello\",3.0]");
		testSamples.push_back("[[],[],[],[5]]");
		testSamples.push_back("[false,false,false,false,false,false,true,10130.1,any,[0.837,0],0,[0,0]]");

		Parameters params;

		for(auto it=testSamples.begin();it!=testSamples.end();++it)
		{
			Value val = lexical_cast<Value>(*it);
			params.push_back(val);
		}

		for (auto it=params.begin();it!=params.end();++it)
		{
			string out = lexical_cast<string>(*it);
			poco_assert(out == testSamples[it-params.begin()]);
		}

		string generatedParams = lexical_cast<string>(params);
		Parameters parsedParameters = lexical_cast<Parameters>(generatedParams);
		string newlyGenerated = lexical_cast<string>(parsedParameters);
		poco_assert(generatedParams == newlyGenerated);

		vector<string> origSampleParams; 
		origSampleParams.push_back("CHILD:302:666:Some String With Spaces::[5.0,[3,5,[]]]:623:");
		origSampleParams.push_back("CHILD:101:14352902:1337:4Fun:");
		for (auto it=origSampleParams.begin();it!=origSampleParams.end();++it)
		{
			parsedParameters = lexical_cast<Parameters>(*it);
			newlyGenerated = lexical_cast<string>(parsedParameters);
			poco_assert(newlyGenerated == *it);
		}
	}
};