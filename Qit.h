#ifndef _Sdb_Qit_
#define _Sdb_Qit_

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
#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>


namespace Sdb
{
	class Record;
	class Transaction;
	class Database;

	class Qit
	{
	public:
		Qit( Record* = 0, Transaction* = 0, quint32 nr = 0 );
		Qit( const Qit& );
		~Qit();

		void setValue( const Stream::DataCell& );
		void getValue( Stream::DataCell& ) const;
		Stream::DataCell getValue() const;
		void erase();
		bool first();
		bool last();
		bool next(); // false..kein Nachfolger, unverändert
		bool prev(); // false..kein Vorgänger, unverängert

		Qit& operator=( const Qit& r ) { return assign( r ); }
		Qit& assign( const Qit& r );
		quint32 getSlotNr() const { return d_nr; }
		Record* getRec() const { return d_rec; }
		Transaction* getTxn() const { return d_txn; }
		bool isNull() const { return d_rec == 0; }
	protected:
		void checkNull() const;
		Record* d_rec;
		Transaction* d_txn;
		quint32 d_nr;
	};
}

#endif
