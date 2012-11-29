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

#include <stdexcept>
#include <Poco/Logger.h>
#include <ostream>

template <typename ExceptType>
class GenericException : public ExceptType
{
public:
	GenericException(const char* staticName) : ExceptType(staticName) {}
	virtual ~GenericException() throw() {}

	virtual std::string toString() const = 0;
	const char* what() const throw() { _str = this->toString(); return _str.c_str(); }
	void print(Poco::Logger& logger) const { logger.error(this->toString()); }
	void print(std::ostream& out) const { out << this->toString() << std::endl; }
private:
	mutable std::string _str;
};

#include <Poco/Exception.h>

#define POCO_DEFINE_EXCEPTION_CODE(API, CLS, BASE, CODE, NAME) \
class API CLS: public BASE																				\
	{																									\
	public:																								\
	CLS(int code = CODE): BASE(code) {}																	\
	CLS(const std::string& msg, int code = CODE): BASE(msg, code) {}									\
	CLS(const std::string& msg, const std::string& arg, int code = CODE): BASE(msg, arg, code) {}		\
	CLS(const std::string& msg, const Poco::Exception& exc, int code = CODE): BASE(msg, exc, code) {}	\
	CLS(const CLS& exc): BASE(exc) {}																	\
	~CLS() throw() {}																					\
	CLS& operator = (const CLS& exc)																	\
	{																									\
		BASE::operator = (exc);																			\
		return *this;																					\
	}																									\
	const char* name() const throw() {return NAME;}														\
	const char* className() const throw() {return typeid(*this).name();}								\
	Poco::Exception* clone() const {return new CLS(*this);}												\
	void rethrow() const {throw *this;}																	\
	};

#define POCO_DEFINE_EXCEPTION(API, CLS, BASE, NAME) \
	POCO_DEFINE_EXCEPTION_CODE(API, CLS, BASE, 0, NAME)

