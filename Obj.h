#ifndef _Sdb_Obj_
#define _Sdb_Obj_

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
#include <Sdb/Lit.h>
#include <Sdb/Rel.h>
#include <Sdb/Qit.h>
#include <Sdb/Mit.h>
#include <QList>

namespace Sdb
{
	// Value
	class Obj : public Orl
	{
	public:
		typedef QList<OID> OidList;
		typedef QList<Stream::DataCell> KeyList;

		Obj( Record* = 0, Transaction* = 0 );
		Obj( const Obj& );
		Obj( const Orl& );

		OID getOid() const { return getId(); }
		void erase();

		// Objektzugriff via Attribute
		Obj getObject( Atom name ) const;
		Rel getRelation( Atom name ) const;
		// -

		// Objektzugriff als Liste
		Lit appendElem( const Stream::DataCell& );
		Lit prependElem( const Stream::DataCell& );
		Lit getFirstElem() const;
		Lit getLastElem() const;
		// - 

		// Objektzugriff als Queue
		Qit appendSlot( const Stream::DataCell& );
		Qit getFirstSlot() const;
		Qit getLastSlot() const;
		Qit getSlot( quint32 nr ) const;
		// -

		// Objektzugriff als Map bzw. MUMPS Global bzw. Sparse Array
		void setCell( const KeyList& key, const Stream::DataCell& value );
		void getCell( const KeyList& key, Stream::DataCell& value ) const;
		Stream::DataCell getCell( const KeyList& key ) const;
		Mit findCells( const KeyList& key ) const;
		// -

		// Unterobjekte
		Obj getOwner() const;
		void aggregateTo(const Obj& owner); // Append
		void deaggregate(); // Befreie dieses Objekt aus dem Aggregat und mache es selbständig
		Obj createAggregate( Atom type = 0 );
		Obj getFirstObj() const;
		Obj getLastObj() const;
		bool next(); // false..kein Nachfolger, unverändert
		bool prev(); // false..kein Vorgänger, unverängert
		void moveBefore( const Obj& target = Obj() ); 
		// -

		// Relationen
		Rel relateTo( const Obj& target, Atom type = 0, bool prepend = true );
		Rel getFirstRel() const;
		Rel getLastRel() const;
		// -

		void dumpElems();
		void dumpRels();
		void dumpObjs();
		operator Stream::DataCell() const;
	private:
		Lit addFirstElem( const Stream::DataCell& );
		void aggregateImp(const Obj& owner); // Append
		void deaggregateImp();
	};
}

#endif
