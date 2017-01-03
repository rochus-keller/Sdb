#ifndef _Sdb_Orl_
#define _Sdb_Orl_

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
#include <Sdb/Globals.h>
#include <QSet>

namespace Sdb
{
	class Record;
	class Transaction;
	class Database;

	// Value
	// Generische Oberklasse für SysObject und SysRelation
	class Orl
	{
	public:
		Orl( Record* = 0, Transaction* = 0 );
		Orl( const Orl& );
		~Orl();

		// Identität
		OID getId() const; // Oid oder Rid, gleicher Werteraum
		bool isRelation() const;
		bool isObject() const;
        bool isDeleted() const;
		void setGuid( const QUuid& );
		QUuid getGuid() const;
		Atom getType() const;
		void setType( Atom );

		// Objektzugriff via Attribute
		void setValue( Atom name, const Stream::DataCell& );
		void getValue( Atom name, Stream::DataCell& ) const;
		bool hasValue( Atom name ) const;
		Stream::DataCell getValue( Atom name ) const;
		typedef QSet<Atom> Names;
		Names getNames() const;
		// -

		Orl& operator=( const Orl& r ) { return assign( r ); }
		Orl& assign( const Orl& r );
		Record* getRec() const { return d_rec; }
		Transaction* getTxn() const { return d_txn; }
		Database* getDb() const;
		bool isNull() const { return d_rec == 0; }
	protected:
		void checkNull() const;
		Record* d_rec;
		Transaction* d_txn;
		void setValuePriv( const Stream::DataCell&, Atom name );
	};
}

#endif
