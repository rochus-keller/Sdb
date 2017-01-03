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

#include "Rel.h"
#include "Record.h"
#include "Transaction.h"
#include "Database.h"
#include "RecordCow.h"
#include "Exceptions.h"
#include <QtDebug>
#include <cassert>
using namespace Sdb;
using namespace Stream;

Rel::Rel( Record* r, Transaction* t ):
	Orl( r, t )
{
}

Rel::Rel( const Rel& lhs ):
	Orl( lhs )
{
}

Rel::Rel( const Orl& lhs )
{
	if( lhs.isRelation() )
		assign( lhs );
}

bool Rel::isTarget(const Obj& obj) const
{
	return getTarget() == obj.getOid();
}

bool Rel::isSource(const Obj& obj) const
{
	return getSource() == obj.getOid();
}

OID Rel::getSource() const
{
	checkNull();
	return d_txn->getIdField( d_rec, Record::FieldSource );
}

OID Rel::getTarget() const
{
	checkNull();
	return d_txn->getIdField( d_rec, Record::FieldTarget );
}

Rel Rel::create( Transaction* txn, const Obj& source, const Obj& target, 
				quint32 type, bool prepend )
{
	assert( txn );
	if( type && type >= Record::MinReservedField )
		throw DatabaseException(DatabaseException::ReservedName );

	if( source.isNull() )
		throw DatabaseException(DatabaseException::InvalidArgument );
	if( target.isNull() )
		throw DatabaseException(DatabaseException::InvalidArgument );

	if( type && type >= Record::MinReservedField )
		throw DatabaseException(DatabaseException::ReservedName );

	Database::Lock lock( txn->getDb() );
	RecordCow* rc = txn->createRecord( Record::TypeRelation );
	if( type )
		txn->setField( rc, Record::FieldType, DataCell().setAtom( type ) );
	txn->setField( rc, Record::FieldSource, source );
	txn->setField( rc, Record::FieldTarget, target );

	Rel rel( rc, txn );

	if( prepend )
		rel.prependTo( source.getRec(), Record::FieldSource );
	else
		rel.appendTo( source.getRec(), Record::FieldSource );
	UpdateInfo c;
	c.d_kind = UpdateInfo::RelationAdded;
	c.d_id = rc->getId();
	c.d_id2 = source.getId();
	c.d_name = type;
	c.d_where = (prepend)?UpdateInfo::First:UpdateInfo::Last;
	c.d_side = UpdateInfo::Source;
	txn->d_notify.append( c );
	// Relationen auf das selbe Objekt dürfen in der Liste nur einmal auftauchen.
	// Sie befinden sich nur in der Source-Liste, bzw. die reflexiven Relationen haben 
	// FieldPrevTarget und FieldNextTarget null.
	if( source.getRec() != target.getRec() )
	{
		if( prepend )
			rel.prependTo( target.getRec(), Record::FieldTarget );
		else
			rel.appendTo( target.getRec(), Record::FieldTarget );
		c.d_kind = UpdateInfo::RelationAdded;
		c.d_id = rc->getId();
		c.d_id2 = target.getId();
		c.d_name = type;
		c.d_where = (prepend)?UpdateInfo::First:UpdateInfo::Last;
		c.d_side = UpdateInfo::Target;
		txn->d_notify.append( c );
	}


	return rel;
}

void Rel::appendTo( Record* obj, quint32 side )
{
	assert( obj );

	Record* last = d_txn->getRecord( 
		d_txn->getIdField( obj, Record::FieldLastRel ), Record::TypeRelation );
	if( last == 0 )
	{
		// obj hat noch keine Relationen
		d_txn->setField( obj, Record::FieldFirstRel, DataCell().setRid( d_rec->getId() ) );
		d_txn->setField( obj, Record::FieldLastRel, DataCell().setRid( d_rec->getId() ) );
	}else
	{
		const OID source = d_txn->getIdField( last, Record::FieldSource );
		assert( obj->getId() == source || obj->getId() == d_txn->getIdField( last, Record::FieldTarget  ) );
		d_txn->setField( last, (obj->getId() == source)?Record::FieldNextSource:
			Record::FieldNextTarget, DataCell().setRid( d_rec->getId() ) );
		d_txn->setField( d_rec, (side==Record::FieldSource)?Record::FieldPrevSource:
			Record::FieldPrevTarget, DataCell().setRid( last->getId() ) );
		d_txn->setField( obj, Record::FieldLastRel, DataCell().setRid( d_rec->getId() ) );
	}
}

void Rel::prependTo( Record* obj, quint32 side )
{
	assert( obj );

	Record* first = d_txn->getRecord( 
		d_txn->getIdField( obj, Record::FieldFirstRel ), Record::TypeRelation );
	if( first == 0 )
	{
		// obj hat noch keine Relationen
		d_txn->setField( obj, Record::FieldFirstRel, DataCell().setRid( d_rec->getId() ) );
		d_txn->setField( obj, Record::FieldLastRel, DataCell().setRid( d_rec->getId() ) );
	}else
	{
		const OID source = d_txn->getIdField( first, Record::FieldSource );
		assert( obj->getId() == source || obj->getId() == d_txn->getIdField( first, Record::FieldTarget  ) );
		d_txn->setField( first, (obj->getId() == source)?Record::FieldPrevSource:
			Record::FieldPrevTarget, DataCell().setRid( d_rec->getId() ) );
		d_txn->setField( d_rec, (side==Record::FieldSource)?Record::FieldNextSource:
			Record::FieldNextTarget, DataCell().setRid( first->getId() ) );
		d_txn->setField( obj, Record::FieldFirstRel, DataCell().setRid( d_rec->getId() ) );
	}
}

Record* Rel::removeFrom( Record* obj )
{
	assert( d_rec );
	assert( obj );

	const bool isSource = obj->getId() == d_txn->getIdField( d_rec, Record::FieldSource );
	assert( isSource || obj->getId() == d_txn->getIdField( d_rec, Record::FieldTarget ) );

	const quint32 fieldNext = (isSource)?Record::FieldNextSource:Record::FieldNextTarget;
	const quint32 fieldPrev = (isSource)?Record::FieldPrevSource:Record::FieldPrevTarget;

	Record* prev = d_txn->getRecord( d_txn->getIdField( d_rec, fieldPrev ), Record::TypeRelation );
	Record* next = d_txn->getRecord( d_txn->getIdField( d_rec, fieldNext ), Record::TypeRelation );
	d_txn->setField( d_rec, fieldPrev, DataCell().setNull() );
	d_txn->setField( d_rec, fieldNext, DataCell().setNull() );
	if( next == 0 || prev == 0 )
	{
		if( prev == 0 && next == 0 )
		{
			d_txn->setField( obj, Record::FieldFirstRel, DataCell().setNull() );
			d_txn->setField( obj, Record::FieldLastRel, DataCell().setNull() );
			return 0;
		}else if( prev == 0 )
		{
			d_txn->setField( obj, Record::FieldFirstRel, DataCell().setRid( next->getId() ) );
			const bool isNextSource = obj->getId() == d_txn->getIdField( next, Record::FieldSource );
			assert( isNextSource || obj->getId() == d_txn->getIdField( next, Record::FieldTarget ) );
			d_txn->setField( next, (isNextSource)?Record::FieldPrevSource:
				Record::FieldPrevTarget, DataCell().setNull() );
			return next;
		}else
		{
			const bool isPrevSource = obj->getId() == d_txn->getIdField( prev, Record::FieldSource );
			assert( isPrevSource || obj->getId() == d_txn->getIdField( prev, Record::FieldTarget ) );
			d_txn->setField( obj, Record::FieldLastRel, DataCell().setRid( prev->getId() ) );
			d_txn->setField( prev, (isPrevSource)?Record::FieldNextSource:
				Record::FieldNextTarget, DataCell().setNull() );
			return prev;
		}
	}else
	{
		const bool isPrevSource = obj->getId() == d_txn->getIdField( prev, Record::FieldSource );
		assert( isPrevSource || obj->getId() == d_txn->getIdField( prev, Record::FieldTarget ) );
		const bool isNextSource = obj->getId() == d_txn->getIdField( next, Record::FieldSource );
		assert( isNextSource || obj->getId() == d_txn->getIdField( next, Record::FieldTarget ) );
		d_txn->setField( prev, (isPrevSource)?Record::FieldNextSource:
				Record::FieldNextTarget, DataCell().setRid( next->getId() ) );
		d_txn->setField( next, (isNextSource)?Record::FieldPrevSource:
				Record::FieldPrevTarget, DataCell().setRid( prev->getId() ) );
		return next;
	}

}

void Rel::erase()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	Record* source = d_txn->getRecord( d_txn->getIdField( d_rec, Record::FieldSource ), 
		Record::TypeObject );
	Record* target = d_txn->getRecord( d_txn->getIdField( d_rec, Record::FieldTarget), 
		Record::TypeObject );
	removeFrom( source );
	if( source != target )
		removeFrom( target );
	d_txn->erase( d_rec );

	UpdateInfo c;
	c.d_kind = UpdateInfo::RelationErased;
	c.d_id = d_rec->getId();
	d_txn->d_notify.append( c );

	// Wir bleiben auf der gelöschten Relation stehen
}

Rel::operator Stream::DataCell() const
{
	Stream::DataCell v;
	if( d_rec )
		v.setRid( getRid() );
	else
		v.setNull();
	return v; 
}

bool Rel::next(OID obj)
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	const bool isSource = obj == d_txn->getIdField( d_rec, Record::FieldSource );
	Record* rel = d_txn->getRecord( d_txn->getIdField( d_rec, 
		(isSource)?Record::FieldNextSource:Record::FieldNextTarget ), Record::TypeRelation );
	if( rel == 0 )
		return false; 
	d_rec->release();
	d_rec = rel;
	d_rec->addRef();
	return true;
}

bool Rel::next(const Obj& obj)
{
	assert( !obj.isNull() );
	return next( obj.getOid() );
}

bool Rel::prev(const Obj& obj)
{
	assert( !obj.isNull() );
	return prev( obj.getOid() );
}

bool Rel::prev(OID obj)
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	const bool isSource = obj == d_txn->getIdField( d_rec, Record::FieldSource );
	Record* rel = d_txn->getRecord( d_txn->getIdField( d_rec, 
		(isSource)?Record::FieldPrevSource:Record::FieldPrevTarget ), Record::TypeRelation );
	if( rel == 0 )
		return false; 
	d_rec->release();
	d_rec = rel;
	d_rec->addRef();
	return true;
}

void Rel::moveBefore( const Obj& obj, const Rel& target )
{
	assert( !obj.isNull() );
	checkNull();
	if( d_rec == target.d_rec )
		return; // Trivialfall, keine Operation
	Database::Lock lock( d_txn->getDb() );
	const bool isSource = obj.getOid() == d_txn->getIdField( d_rec, Record::FieldSource );
	assert( isSource || obj.getOid() == d_txn->getIdField( d_rec, Record::FieldTarget ) );
	if( target.isNull() )
	{
		// Verschiebe d_rec an das Ende der Liste
		if( d_txn->getIdField( d_rec, 
			(isSource)?Record::FieldNextSource:Record::FieldNextTarget) == 0 )
			return; // Trivialfall, keine Operation

		removeFrom( obj.getRec() );

		Record* oldLast = d_txn->getRecord( 
			d_txn->getIdField( obj.getRec(), Record::FieldLastRel ), Record::TypeRelation );
		if( oldLast == 0 )
			throw DatabaseException( DatabaseException::RecordFormat );

		const bool isLastSource = 
			obj.getOid() == d_txn->getIdField( oldLast, Record::FieldSource );
		d_txn->setField( oldLast, (isLastSource)?Record::FieldNextSource:
			Record::FieldNextTarget, DataCell().setRid( d_rec->getId() ) );
		d_txn->setField( d_rec, (isSource)?Record::FieldPrevSource:
			Record::FieldPrevTarget, DataCell().setRid( oldLast->getId() ) );
		d_txn->setField( d_rec, (isSource)?Record::FieldNextSource:
			Record::FieldNextTarget, DataCell().setNull() );

		d_txn->setField( obj.getRec(), Record::FieldLastRel, DataCell().setRid( d_rec->getId() ) );
		
		UpdateInfo c;
		c.d_kind = UpdateInfo::RelationMoved;
		c.d_id = d_rec->getId();
		c.d_where = UpdateInfo::Last;
		c.d_side = (isSource)?UpdateInfo::Source:UpdateInfo::Target;
		d_txn->d_notify.append( c );
	}else
	{
		Record* next = target.d_rec;
		const bool isNextSource = 
			obj.getOid() == d_txn->getIdField( next, Record::FieldSource );
		if( !isNextSource && obj.getOid() != d_txn->getIdField( next, Record::FieldTarget) )
			throw DatabaseException( DatabaseException::WrongContext );
		Record* prev = d_txn->getRecord( d_txn->getIdField( next, (isNextSource)?
			Record::FieldPrevSource:Record::FieldPrevTarget ), Record::TypeRelation );
		if( prev == d_rec )
			return; // Trivialfall, keine Operation

		removeFrom( obj.getRec() );

		if( prev == 0 )
		{
			// Target ist das erste Element in der Liste
			d_txn->setField( next, (isNextSource)?Record::FieldPrevSource:
				Record::FieldPrevTarget, DataCell().setRid( d_rec->getId() ) );
			d_txn->setField( d_rec, (isSource)?Record::FieldNextSource:
				Record::FieldNextTarget, DataCell().setRid( next->getId() ) );
			d_txn->setField( d_rec, (isSource)?Record::FieldPrevSource:
				Record::FieldPrevTarget, DataCell().setNull() );
			d_txn->setField( obj.getRec(), Record::FieldFirstRel, 
				DataCell().setRid( d_rec->getId() ) );

			UpdateInfo c;
			c.d_kind = UpdateInfo::RelationMoved;
			c.d_id = d_rec->getId();
			c.d_where = UpdateInfo::First;
			c.d_side = (isSource)?UpdateInfo::Source:UpdateInfo::Target;
			d_txn->d_notify.append( c );
		}else
		{
			const bool isPrevSource = 
				obj.getOid() == d_txn->getIdField( prev, Record::FieldSource );
			d_txn->setField( next, (isNextSource)?Record::FieldPrevSource:
				Record::FieldPrevTarget, DataCell().setRid( d_rec->getId() ) );
			d_txn->setField( prev, (isPrevSource)?Record::FieldNextSource:
				Record::FieldNextTarget, DataCell().setRid( d_rec->getId() ) );
			d_txn->setField( d_rec, (isSource)?Record::FieldPrevSource:
				Record::FieldPrevTarget, DataCell().setRid( prev->getId() ) );
			d_txn->setField( d_rec, (isSource)?Record::FieldNextSource:
				Record::FieldNextTarget, DataCell().setRid( next->getId() ) );

			UpdateInfo c;
			c.d_kind = UpdateInfo::RelationMoved;
			c.d_id = d_rec->getId();
			c.d_id2 = next->getId();
			c.d_where = UpdateInfo::Before;
			c.d_side = (isSource)?UpdateInfo::Source:UpdateInfo::Target;
			d_txn->d_notify.append( c );
		}
	}
}
