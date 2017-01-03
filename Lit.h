#ifndef _Sdb_Lit_
#define _Sdb_Lit_

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


#include <Stream/DataCell.h>
#include <Sdb/Globals.h>

namespace Sdb
{
	class Record;
	class Transaction;

	class Lit // List Iterator
	{
	public:
		Lit( Record* elem = 0, Transaction* = 0 );
		Lit( const Lit& );
		~Lit();

		OID getBookmark() const;
		void erase();

		void setValue( const Stream::DataCell& );
		void getValue( Stream::DataCell& ) const;
		Stream::DataCell getValue() const;
		void insertBefore( const Stream::DataCell& ); // vor aktueller pos
		void insertAfter( const Stream::DataCell& ); // nach aktueller pos

		// verschiebe aktuelle pos vor target oder an Schluss wenn target.isNull()
		void moveBefore( const Lit& target = Lit() ); 

		bool next(); // false..kein Nachfolger, unverändert
		bool prev(); // false..kein Vorgänger, unverängert

		Lit& operator=( const Lit& r );
		bool isNull() const { return d_elem == 0; }
		bool equals( const Lit& ) const;
		void dump();
	private:
		void checkNull() const;
		Record* removeCurrentFromList();
		Record* getListOfElem( Record* ) const;
		Record* d_elem;
		Transaction* d_txn;
	};
}

#endif // _Sdb_Lit_
