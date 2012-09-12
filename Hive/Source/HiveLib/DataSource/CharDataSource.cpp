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

#include "CharDataSource.h"

#include <boost/algorithm/string/predicate.hpp>

namespace
{
	enum MeleeAmmoType
	{
		MELEE_HATCHET,
		MELEE_CROWBAR,
		MELEE_COUNT
	};

	class MeleeAmmoVisitor : public boost::static_visitor<MeleeAmmoType>
	{
	public:
		MeleeAmmoType operator()(const string& itemClass) const
		{
			if (boost::iequals(itemClass,"Hatchet_Swing"))
				return MELEE_HATCHET;
			else if (boost::iequals(itemClass,"crowbar_swing"))
				return MELEE_CROWBAR;
			else
				return MELEE_COUNT;
		}
		template<typename T> MeleeAmmoType operator()(const T& other) const	{ return MELEE_COUNT; }
	};
}

int CharDataSource::SanitiseInv( Sqf::Parameters& origInv )
{
	if (origInv.size() != 2) //empty inv or not conforming to [weapons, magazines] layout
		return 0;

	map<MeleeAmmoType,int> numAmmo;
	numAmmo[MELEE_HATCHET] = 0;
	numAmmo[MELEE_CROWBAR] = 0;

	try
	{
		int numErased = 0;

		Sqf::Parameters& magazines = boost::get<Sqf::Parameters>(origInv.at(1));
		for (auto it=magazines.begin();it!=magazines.end();)
		{
			MeleeAmmoType ammoType = boost::apply_visitor(MeleeAmmoVisitor(),*it);
			if (ammoType != MELEE_COUNT)
			{
				++numAmmo[ammoType];

				if (numAmmo[ammoType] > 1) //erase all but 1st
				{
					it = magazines.erase(it);
					numErased++;
				}
				else
					++it;
			}
			else
				++it;
		}

		return numErased;
	}
	catch (const boost::bad_get&) { return 0; } //magazines not an array?
}