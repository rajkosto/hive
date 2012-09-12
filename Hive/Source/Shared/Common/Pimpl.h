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

#include <memory>

template<typename T>
class Pimpl 
{
private:
	std::unique_ptr<T> m;
public:
	Pimpl();
	template<typename Arg1>
	Pimpl( Arg1&& arg1 );

	template<typename Arg1, typename Arg2>
	Pimpl( Arg1&& arg1, Arg2&& arg2 );

	template<typename Arg1, typename Arg2, typename Arg3>
	Pimpl( Arg1&& arg1, Arg2&& arg2, Arg3&& arg3 );

	~Pimpl();

	T* operator->();
	T* operator->() const;
	T& operator*();
	T& operator*() const;
};