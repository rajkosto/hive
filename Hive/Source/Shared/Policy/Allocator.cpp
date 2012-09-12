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

#ifndef USE_STANDARD_MALLOC

#include <tbb/scalable_allocator.h>

#pragma warning( disable : 4290 )

// No retry loop because we assume that scalable_malloc does
// all it takes to allocate the memory, so calling it repeatedly
// will not improve the situation at all
//
// No use of std::new_handler because it cannot be done in portable
// and thread-safe way (see sidebar)
//
// We throw std::bad_alloc() when scalable_malloc returns NULL
//(we return NULL if it is a no-throw implementation)
void* operator new (size_t size) throw (std::bad_alloc)
{
	if (size == 0) size = 1;
	if (void* ptr = scalable_malloc (size))
		return ptr;
	throw std::bad_alloc ();
}
void* operator new[] (size_t size) throw (std::bad_alloc)
{
	return operator new (size);
}
void* operator new (size_t size, const std::nothrow_t&) throw ()
{
	if (size == 0) size = 1;
	if (void* ptr = scalable_malloc (size))
		return ptr;
	return NULL;
}

void* operator new[] (size_t size, const std::nothrow_t&) throw ()
{
	return operator new (size, std::nothrow);
}
void operator delete (void* ptr) throw ()
{
	if (ptr != 0) scalable_free (ptr);
}
void operator delete[] (void* ptr) throw ()
{
	operator delete (ptr);
}
void operator delete (void* ptr, const std::nothrow_t&) throw ()
{
	if (ptr != 0) scalable_free (ptr);
}
void operator delete[] (void* ptr, const std::nothrow_t&) throw ()
{
	operator delete (ptr, std::nothrow);
}

#endif
