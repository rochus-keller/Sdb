#ifndef _Sdb_Rel_
#define _Sdb_Rel_

/*
* Copyright 2005-2017 Rochus Keller <mailto:me@rochus-keller.info>
*
* This file is part of the DoorScope Sdb library.
*
* The following is the license that applies to this copy of the
* library. For a license to use the library under conditions
* other than those described here, please email to me@rochus-keller.info.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include <Sdb/Orl.h>

namespace Sdb
{
	class Obj;

	// Dient gleichzeitig als Iterator
	class Rel : public Orl
	{
	public:
		Rel( Record* = 0, Transaction* = 0 );
		Rel( const Rel& );
		Rel( const Orl& );

		OID getTarget() const; // Relation oder Object
		OID getSource() const; // Relation oder Object
		OID getRid() const { return getId(); }
		bool isTarget(const Obj& obj) const;
		bool isSource(const Obj& obj) const;
		void erase();

		void moveBefore( const Obj& obj, const Rel& target = Rel() ); 

		bool next(OID obj); // false..kein Nachfolger, unverändert
		bool next(const Obj& obj);
		bool prev(OID obj); // false..kein Vorgänger, unverängert
		bool prev(const Obj& obj);

		operator Stream::DataCell() const;
		static Rel create( Transaction*, const Obj& source, const Obj& target, 
			Atom type = 0, bool prepend = true );
	private:
		void appendTo( Record* obj, Atom side );
		void prependTo( Record* obj, Atom side );
		Record* removeFrom( Record* obj );
	};
}

#endif // _Sdb_Rel_
