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

#include "Obj.h"
#include "Record.h"
#include "Transaction.h"
#include "Database.h"
#include "RecordCow.h"
#include "Exceptions.h"
#include <QtDebug>
using namespace Sdb;
using namespace Stream;

Obj::Obj( Record* r, Transaction* t ):
	Orl( r, t )
{
}

Obj::Obj( const Obj& lhs ):
	Orl( lhs )
{
}

Obj::Obj( const Orl& lhs )
{
	if( lhs.isObject() )
		assign( lhs );
}

void Obj::erase()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	Lit lit = getFirstElem();
	if( !lit.isNull() ) do
	{
		lit.erase();
	}while( lit.next() );
	Rel rel = getFirstRel();
	if( !rel.isNull() ) do
	{
		Rel r = rel;
		const bool hasNext = rel.next(getId());
		r.erase();
		if( !hasNext )
			break;
	}while( true );
	Obj obj = getFirstObj();
	if( !obj.isNull() ) do
	{
		Obj o = obj;
		const bool hasNext = obj.next();
		o.erase();
		if( !hasNext )
			break;
	}while( true );
	deaggregateImp();
	const quint32 type = getType();
	d_txn->erase( d_rec );

	UpdateInfo c;
	c.d_kind = UpdateInfo::ObjectErased;
	c.d_id = d_rec->getId();
	c.d_name = type;
	d_txn->d_notify.append( c );
}

Obj::operator Stream::DataCell() const
{
	Stream::DataCell v;
	if( d_rec )
		v.setOid( getOid() );
	else
		v.setNull();
	return v; 
}

Lit Obj::addFirstElem( const Stream::DataCell& v )
{
	RecordCow* elem = d_txn->createRecord( Record::TypeElement );
	d_txn->setField( elem, Record::FieldValue, v );
	d_txn->setField( elem, Record::FieldList, DataCell().setOid( d_rec->getId() ) );
	d_txn->setField( d_rec, Record::FieldFirstElm, DataCell().setId64( elem->getId() ) );
	d_txn->setField( d_rec, Record::FieldLastElm, DataCell().setId64( elem->getId() ) );

	UpdateInfo c;
	c.d_kind = UpdateInfo::ElementAdded;
	c.d_id = elem->getId();
	c.d_id2 = d_rec->getId();
	c.d_where = UpdateInfo::First;
	d_txn->d_notify.append( c );
	return Lit( elem, d_txn );
}

Lit Obj::appendElem( const Stream::DataCell& v )
{
	Database::Lock lock( d_txn->getDb() );
	Lit last = getLastElem();
	if( !last.isNull() )
	{
		last.insertAfter( v );
		last.next();
		return last;
	}else
		return addFirstElem( v );
}

Lit Obj::prependElem( const Stream::DataCell& v )
{
	Database::Lock lock( d_txn->getDb() );
	Lit first = getFirstElem();
	if( !first.isNull() )
	{
		first.insertBefore( v );
		first.prev();
		return first;
	}else
		return addFirstElem( v );
}

Lit Obj::getFirstElem() const
{
	if( d_rec == 0 )
		return Lit();
	Database::Lock lock( d_txn->getDb() );
	DataCell id;
	d_txn->getField( d_rec, Record::FieldFirstElm, id );
	Record* firstElem = d_txn->getRecord( id.getId64(), Record::TypeElement );
	if( firstElem == 0 )
		return Lit();
	else
		return Lit( firstElem, d_txn );
}

Lit Obj::getLastElem() const
{
	if( d_rec == 0 )
		return Lit();
	Database::Lock lock( d_txn->getDb() );
	DataCell id;
	d_txn->getField( d_rec, Record::FieldLastElm, id );
	Record* lastElem = d_txn->getRecord( id.getId64(), Record::TypeElement );
	if( lastElem == 0 )
		return Lit();
	else
		return Lit( lastElem, d_txn );
}

Rel Obj::getFirstRel() const
{
	if( d_rec == 0 )
		return Rel();
	Database::Lock lock( d_txn->getDb() );
	DataCell id;
	d_txn->getField( d_rec, Record::FieldFirstRel, id );
	Record* rel = d_txn->getRecord( id.getRid(), Record::TypeRelation );
	if( rel == 0 )
		return Rel();
	else
		return Rel( rel, d_txn );
}

Rel Obj::getLastRel() const
{
	if( d_rec == 0 )
		return Rel();
	Database::Lock lock( d_txn->getDb() );
	DataCell id;
	d_txn->getField( d_rec, Record::FieldLastRel, id );
	Record* rel = d_txn->getRecord( id.getRid(), Record::TypeRelation );
	if( rel == 0 )
		return Rel();
	else
		return Rel( rel, d_txn );
}

void Obj::dumpElems()
{
	checkNull();
	qDebug() << "#### List of oid=" << d_rec->getId();
	Sdb::Lit l = getFirstElem();
	if( !l.isNull() ) do
	{
		DataCell v;
		l.getValue( v );
		qDebug() << v.toPrettyString();
	}while( l.next() );
	qDebug() << "#### End List";
}

void Obj::dumpRels()
{
	checkNull();
	qDebug() << "#### Rels of oid=" << d_rec->getId();
	Sdb::Rel l = getFirstRel();
	if( !l.isNull() ) do
	{
		qDebug() << "rid("<< l.getRid() << "): oid(" << l.getSource() << 
			") -> oid(" << l.getTarget() << ")";
	}while( l.next( getOid() ) );
	qDebug() << "#### End Rels";
}

void Obj::dumpObjs()
{
	checkNull();
	qDebug() << "#### Sub-Obs of oid=" << d_rec->getId();
	Obj l = getFirstObj();
	if( !l.isNull() ) do
	{
		qDebug() << "oid("<< l.getOid() << ")";
	}while( l.next() );
	qDebug() << "#### End Sub-Obs";
}

Rel Obj::relateTo( const Obj& target, quint32 type, bool prepend )
{
	checkNull();
	if( target.isNull() )
		return Rel();
	return Rel::create( d_txn, *this, target, type, prepend );
}

Obj Obj::getFirstObj() const
{
	if( d_rec == 0 )
		return Obj();
	Database::Lock lock( d_txn->getDb() );
	DataCell id;
	d_txn->getField( d_rec, Record::FieldFirstObj, id );
	Record* obj = d_txn->getRecord( id.getOid(), Record::TypeObject );
	if( obj == 0 )
		return Obj();
	else
		return Obj( obj, d_txn );
}

Obj Obj::getLastObj() const
{
	if( d_rec == 0 )
		return Obj();
	Database::Lock lock( d_txn->getDb() );
	DataCell id;
	d_txn->getField( d_rec, Record::FieldLastObj, id );
	Record* obj = d_txn->getRecord( id.getOid(), Record::TypeObject );
	if( obj == 0 )
		return Obj();
	else
		return Obj( obj, d_txn );
}

bool Obj::next()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	Record* obj = d_txn->getRecord( 
		d_txn->getIdField( d_rec, Record::FieldNextObj ), Record::TypeObject );
	if( obj == 0 )
		return false; 
	d_rec->release();
	d_rec = obj;
	d_rec->addRef();
	return true;
}

bool Obj::prev()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	Record* obj = d_txn->getRecord( 
		d_txn->getIdField( d_rec, Record::FieldPrevObj ), Record::TypeObject );
	if( obj == 0 )
		return false; 
	d_rec->release();
	d_rec = obj;
	d_rec->addRef();
	return true;
}

Obj Obj::getOwner() const
{
	checkNull();
	Record* owner = d_txn->getRecord( 
		d_txn->getIdField( d_rec, Record::FieldOwner ), Record::TypeObject );
	if( owner == 0 )
		return Obj();
	else
		return Obj( owner, d_txn );
}

void Obj::deaggregateImp()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	Record* owner = getOwner().getRec();
	if( owner == 0 )
		return;
	Record* prev = d_txn->getRecord( 
		d_txn->getIdField( d_rec, Record::FieldPrevObj ), Record::TypeObject );
	Record* next = d_txn->getRecord( 
		d_txn->getIdField( d_rec, Record::FieldNextObj ), Record::TypeObject );
	if( next == 0 || prev == 0 )
	{
		if( prev == 0 && next == 0 )
		{
			d_txn->setField( owner, Record::FieldFirstObj, DataCell().setNull() );
			d_txn->setField( owner, Record::FieldLastObj, DataCell().setNull() );
		}else if( prev == 0 )
		{
			d_txn->setField( owner, Record::FieldFirstObj, DataCell().setOid( next->getId() ) );
			d_txn->setField( next, Record::FieldPrevObj, DataCell().setNull() );
		}else
		{
			d_txn->setField( owner, Record::FieldLastObj, DataCell().setOid( prev->getId() ) );
			d_txn->setField( prev, Record::FieldNextObj, DataCell().setNull() );
		}
	}else
	{
		d_txn->setField( prev, Record::FieldNextObj, DataCell().setOid( next->getId() ) );
		d_txn->setField( next, Record::FieldPrevObj, DataCell().setOid( prev->getId() ) );
	}
	d_txn->setField( d_rec, Record::FieldPrevObj, DataCell().setNull() );
	d_txn->setField( d_rec, Record::FieldNextObj, DataCell().setNull() );
	d_txn->setField( d_rec, Record::FieldOwner, DataCell().setNull() );
}

void Obj::aggregateTo(const Obj& owner)
{
	aggregateImp( owner );
	UpdateInfo c;
	c.d_kind = UpdateInfo::Aggregated;
	c.d_id = d_rec->getId();
	c.d_id2 = owner.getId();
	c.d_where = UpdateInfo::Last;
	d_txn->d_notify.append( c );
}

void Obj::aggregateImp(const Obj& owner)
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	deaggregateImp();

	if( owner.isNull() )
		return;

	d_txn->setField( d_rec, Record::FieldOwner, owner );

	Record* obj = owner.getRec();
	Record* last = d_txn->getRecord( d_txn->getIdField( obj, Record::FieldLastObj ), 
		Record::TypeObject );
	if( last == 0 )
	{
		// obj hat noch keine Aggregate
		d_txn->setField( obj, Record::FieldFirstObj, DataCell().setOid( d_rec->getId() ) );
		d_txn->setField( obj, Record::FieldLastObj, DataCell().setOid( d_rec->getId() ) );
	}else
	{
		d_txn->setField( last, Record::FieldNextObj, DataCell().setOid( d_rec->getId() ) );
		d_txn->setField( d_rec, Record::FieldPrevObj, DataCell().setOid( last->getId() ) );
		d_txn->setField( obj, Record::FieldLastObj, DataCell().setOid( d_rec->getId() ) );
	}
}

void Obj::deaggregate()
{
	const OID owner = d_txn->getIdField( d_rec, Record::FieldOwner );
	if( owner == 0 )
		return;
	deaggregateImp();
	UpdateInfo c;
	c.d_kind = UpdateInfo::Deaggregated;
	c.d_id = d_rec->getId();
	c.d_id2 = owner;
	d_txn->d_notify.append( c );
}

Obj Obj::createAggregate(Atom type)
{
	checkNull();
	Obj sub = d_txn->createObject( type );
	sub.aggregateTo( *this );
	return sub;
}

void Obj::moveBefore( const Obj& target )
{
	checkNull();
	if( d_rec == target.d_rec )
		return; // Trivialfall, keine Operation
	Database::Lock lock( d_txn->getDb() );
	if( target.isNull() )
	{
		// Verschiebe d_rec an das Ende der Liste
		if( d_txn->getIdField( d_rec, Record::FieldNextObj ) == 0 )
			return; // Trivialfall, keine Operation

		Record* owner = getOwner().getRec();
		if( owner == 0 )
			return;
		deaggregateImp();
		aggregateImp( Obj( owner, d_txn ) );

		UpdateInfo c;
		c.d_kind = UpdateInfo::AggregateMoved;
		c.d_id = d_rec->getId();
		c.d_where = UpdateInfo::Last;
		d_txn->d_notify.append( c );
	}else
	{
		Record* owner = target.getOwner().getRec();
		if( owner == 0 )
			// Ziel ist gar nicht Element eines Aggregats
			throw DatabaseException( DatabaseException::WrongContext );

		Record* next = target.d_rec;
		Record* prev = d_txn->getRecord( 
			d_txn->getIdField( next, Record::FieldPrevObj ), Record::TypeObject );
		if( prev == d_rec )
			return; // Trivialfall, keine Operation

		deaggregateImp();
		d_txn->setField( d_rec, Record::FieldOwner, DataCell().setOid( owner->getId() ) );

		if( prev == 0 )
		{
			// Target ist das erste Element in der Liste
			d_txn->setField( next, Record::FieldPrevObj, DataCell().setOid( d_rec->getId() ) );
			d_txn->setField( d_rec, Record::FieldNextObj, DataCell().setOid( next->getId() ) );
			d_txn->setField( d_rec, Record::FieldPrevObj, DataCell().setNull() );
			d_txn->setField( owner, Record::FieldFirstObj, DataCell().setOid( d_rec->getId() ) );

			UpdateInfo c;
			c.d_kind = UpdateInfo::AggregateMoved;
			c.d_id = d_rec->getId();
			c.d_where = UpdateInfo::First;
			d_txn->d_notify.append( c );
		}else
		{
			d_txn->setField( next, Record::FieldPrevObj, DataCell().setOid( d_rec->getId() ) );
			d_txn->setField( prev, Record::FieldNextObj, DataCell().setOid( d_rec->getId() ) );
			d_txn->setField( d_rec, Record::FieldPrevObj, DataCell().setOid( prev->getId() ) );
			d_txn->setField( d_rec, Record::FieldNextObj, DataCell().setOid( next->getId() ) );

			UpdateInfo c;
			c.d_kind = UpdateInfo::AggregateMoved;
			c.d_id = d_rec->getId();
			c.d_id2 = next->getId();
			c.d_where = UpdateInfo::Before;
			d_txn->d_notify.append( c );
		}
	}
}

Qit Obj::getFirstSlot() const
{
	if( d_rec == 0 )
		return Qit();
	Qit q( d_rec, d_txn );
	if( q.first() )
		return q;
	else
		return Qit();
}

Qit Obj::getLastSlot() const
{
	if( d_rec == 0 )
		return Qit();
	Qit q( d_rec, d_txn );
	if( q.last() )
		return q;
	else
		return Qit();
}

Qit Obj::appendSlot( const Stream::DataCell& cell )
{
	if( d_rec == 0 )
		return Qit();
	Database::Lock lock( d_txn->getDb(), true );
	const quint32 nr = d_txn->createQSlot( d_rec, cell );
	UpdateInfo c;
	c.d_kind = UpdateInfo::QueueAdded;
	c.d_id = nr;
	c.d_id2 = d_rec->getId();
	d_txn->d_notify.append( c );
	return Qit( d_rec, d_txn, nr );
}

Qit Obj::getSlot( quint32 nr ) const
{
	if( d_rec == 0 )
		return Qit();
	DataCell maxnr;
	d_txn->getQSlot( d_rec, 0, maxnr );
	if( nr > maxnr.getId32() )
		return Qit();
	else
		return Qit( d_rec, d_txn, nr );
}

Obj Obj::getObject( quint32 name ) const
{
	return getTxn()->getObject( getValue( name ) );
}

Rel Obj::getRelation( quint32 name ) const
{
	return getTxn()->getRelation( getValue( name ) );
}

void Obj::setCell( const QList<Stream::DataCell>& key, const Stream::DataCell& value )
{
	Database::Lock lock( d_txn->getDb(), true );
	d_txn->setCell( d_rec, key, value );
	/* TODO
	UpdateInfo c;
	c.d_kind = UpdateInfo::QueueAdded;
	c.d_id = d_rec->getId();
	c.d_id2 = d_rec->getId(); // TODO: key
	d_txn->d_notify.append( c );
	*/
}

void Obj::getCell( const QList<Stream::DataCell>& key, Stream::DataCell& value ) const
{
	value.setNull();
	if( d_rec == 0 )
		return;
	d_txn->getCell( d_rec, key, value );
}

Stream::DataCell Obj::getCell( const QList<Stream::DataCell>& key ) const
{
	Stream::DataCell v;
	getCell( key, v );
	return v;	
}

Mit Obj::findCells( const KeyList& key ) const
{
	if( d_rec == 0 )
		return Mit();
	Mit m( d_rec, d_txn );
	m.seek( key );
	return m;
}
