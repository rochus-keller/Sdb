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

#include "Lit.h"
#include "Record.h"
#include "Transaction.h"
#include "Database.h"
#include "RecordCow.h"
#include "Exceptions.h"
#include <QBuffer>
using namespace Sdb;
using namespace Stream;

Lit::Lit( Record* elem , Transaction* txn )
{
	d_elem = elem;
	d_txn = txn;
	if( d_elem )
		d_elem->addRef();
}

Lit::Lit( const Lit& lhs )
{
	d_elem = 0;
	d_txn = 0;
	*this = lhs;
}

Lit::~Lit()
{
	if( d_elem )
		d_elem->release();
}

Lit& Lit::operator=( const Lit& r )
{
	if( d_elem == r.d_elem )
		return *this;
	if( d_elem )
		d_elem->release();
	d_elem = r.d_elem;
	d_txn = r.d_txn;
	if( d_elem )
		d_elem->addRef();
	return *this;
}	

bool Lit::equals( const Lit& rhs ) const
{
	if( d_elem == 0 || rhs.d_elem == 0 )
		return false;
	return d_txn == rhs.d_txn && d_elem->getId() == rhs.d_elem->getId();
}

void Lit::checkNull() const
{
	if( d_elem == 0 )
		throw DatabaseException(DatabaseException::AccessRecord, "null");
}

void Lit::setValue( const Stream::DataCell& cell )
{
	checkNull();
	d_txn->setField( d_elem, Record::FieldValue, cell );
	UpdateInfo c;
	c.d_kind = UpdateInfo::ElementChanged;
	c.d_id = d_elem->getId();
	d_txn->d_notify.append( c );
}

void Lit::getValue( Stream::DataCell& cell ) const
{
	checkNull();
	d_txn->getField( d_elem, Record::FieldValue, cell );
}

Stream::DataCell Lit::getValue() const
{
	Stream::DataCell v;
	getValue( v );
	return v;
}

bool Lit::next()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	Record* elem = d_txn->getRecord( 
		d_txn->getIdField( d_elem, Record::FieldNextElem ), Record::TypeElement );
	if( elem == 0 )
		return false; 
	d_elem->release();
	d_elem = elem;
	d_elem->addRef();
	return true;
}

bool Lit::prev()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	Record* elem = d_txn->getRecord( 
		d_txn->getIdField( d_elem, Record::FieldPrevElem ), Record::TypeElement );
	if( elem == 0 )
		return false; 
	d_elem->release();
	d_elem = elem;
	d_elem->addRef();
	return true;
}

OID Lit::getBookmark() const
{
	checkNull();
	return d_elem->getId();
}

Record* Lit::getListOfElem( Record* r ) const
{
	DataCell id;
	d_txn->getField( r, Record::FieldList, id );
	Record* list = d_txn->getRecord( id.getOid(), Record::TypeObject );
	if( list == 0 )
		throw DatabaseException( DatabaseException::RecordFormat );
	return list;
}

Record* Lit::removeCurrentFromList()
{
	Record* prev = d_txn->getRecord( 
		d_txn->getIdField( d_elem, Record::FieldPrevElem ), Record::TypeElement );
	Record* next = d_txn->getRecord( 
		d_txn->getIdField( d_elem, Record::FieldNextElem ), Record::TypeElement );
	if( next == 0 || prev == 0 )
	{
		Record* list = getListOfElem( d_elem );
		if( prev == 0 && next == 0 )
		{
			d_txn->setField( list, Record::FieldFirstElm, DataCell().setNull() );
			d_txn->setField( list, Record::FieldLastElm, DataCell().setNull() );
			return 0;
		}else if( prev == 0 )
		{
			d_txn->setField( list, Record::FieldFirstElm, DataCell().setId64( next->getId() ) );
			d_txn->setField( next, Record::FieldPrevElem, DataCell().setNull() );
			return next;
		}else
		{
			d_txn->setField( list, Record::FieldLastElm, DataCell().setId64( prev->getId() ) );
			d_txn->setField( prev, Record::FieldNextElem, DataCell().setNull() );
			return prev;
		}
	}else
	{
		d_txn->setField( prev, Record::FieldNextElem, DataCell().setId64( next->getId() ) );
		d_txn->setField( next, Record::FieldPrevElem, DataCell().setId64( prev->getId() ) );
		return next;
	}
}

void Lit::erase()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	Record* replace = removeCurrentFromList();
	d_txn->erase( d_elem );

	UpdateInfo c;
	c.d_kind = UpdateInfo::ElementChanged;
	c.d_id = d_elem->getId();
	d_txn->d_notify.append( c );

	d_elem->release();
	d_elem = replace;
	if( d_elem )
		d_elem->addRef();
}

void Lit::insertBefore( const Stream::DataCell& v )
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	RecordCow* elem = d_txn->createRecord( Record::TypeElement );
	d_txn->setField( elem, Record::FieldValue, v );
	const OID prevId = d_txn->getIdField( d_elem, Record::FieldPrevElem );
	Record* list = getListOfElem( d_elem );
	if( prevId == 0 )
	{
		// Wir sind das erste Element in der Liste
		d_txn->setField( d_elem, Record::FieldPrevElem, DataCell().setId64( elem->getId() ) );
		d_txn->setField( elem, Record::FieldNextElem, DataCell().setId64( d_elem->getId() ) );
		d_txn->setField( list, Record::FieldFirstElm, DataCell().setId64( elem->getId() ) );
		d_txn->setField( elem, Record::FieldList, DataCell().setOid( list->getId() ) );

		UpdateInfo c;
		c.d_kind = UpdateInfo::ElementAdded;
		c.d_id = d_elem->getId();
		c.d_id2 = list->getId();
		c.d_where = UpdateInfo::First;
		d_txn->d_notify.append( c );
	}else
	{
		Record* prevElem = d_txn->getRecord( prevId, Record::TypeElement );
		if( prevElem == 0 )
			throw DatabaseException( DatabaseException::RecordFormat );
		d_txn->setField( elem, Record::FieldPrevElem, DataCell().setId64( prevId ) );
		d_txn->setField( prevElem, Record::FieldNextElem, DataCell().setId64( elem->getId() ) );
		d_txn->setField( d_elem, Record::FieldPrevElem, DataCell().setId64( elem->getId() ) );
		d_txn->setField( elem, Record::FieldNextElem, DataCell().setId64( d_elem->getId() ) );
		d_txn->setField( elem, Record::FieldList, 
			DataCell().setOid( d_txn->getIdField( d_elem, Record::FieldList ) ) );

		UpdateInfo c;
		c.d_kind = UpdateInfo::ElementAdded;
		c.d_id = d_elem->getId();
		c.d_id2 = list->getId();
		d_txn->d_notify.append( c );
	}
}

void Lit::insertAfter( const Stream::DataCell& v )
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	RecordCow* newElem = d_txn->createRecord( Record::TypeElement );
	d_txn->setField( newElem, Record::FieldValue, v );
	const OID nextId = d_txn->getIdField( d_elem, Record::FieldNextElem );
	Record* list = getListOfElem( d_elem );
	if( nextId == 0 )
	{
		// Wir sind das letzte Element in der Liste
		d_txn->setField( d_elem, Record::FieldNextElem, DataCell().setId64( newElem->getId() ) );
		d_txn->setField( newElem, Record::FieldPrevElem, DataCell().setId64( d_elem->getId() ) );
		d_txn->setField( list, Record::FieldLastElm, DataCell().setId64( newElem->getId() ) );
		d_txn->setField( newElem, Record::FieldList, DataCell().setOid( list->getId() ) );

		UpdateInfo c;
		c.d_kind = UpdateInfo::ElementAdded;
		c.d_id = d_elem->getId();
		c.d_id2 = list->getId();
		c.d_where = UpdateInfo::Last;
		d_txn->d_notify.append( c );
	}else
	{
		Record* nextElem = d_txn->getRecord( nextId, Record::TypeElement );
		if( nextElem == 0 )
			throw DatabaseException( DatabaseException::RecordFormat );
		d_txn->setField( newElem, Record::FieldNextElem, DataCell().setId64( nextId ) );
		d_txn->setField( nextElem, Record::FieldPrevElem, DataCell().setId64( newElem->getId() ) );
		d_txn->setField( d_elem, Record::FieldNextElem, DataCell().setId64( newElem->getId() ) );
		d_txn->setField( newElem, Record::FieldPrevElem, DataCell().setId64( d_elem->getId() ) );
		d_txn->setField( newElem, Record::FieldList, 
			DataCell().setOid( d_txn->getIdField( d_elem, Record::FieldList ) ) );
		UpdateInfo c;
		c.d_kind = UpdateInfo::ElementAdded;
		c.d_id = d_elem->getId();
		c.d_id2 = list->getId();
		d_txn->d_notify.append( c );
	}
}

void Lit::moveBefore( const Lit& target )
{
	checkNull();
	if( d_elem == target.d_elem )
		return; // Trivialfall, keine Operation
	Database::Lock lock( d_txn->getDb() );
	if( target.isNull() )
	{
		// Verschiebe d_elem an das Ende der Liste
		if( d_txn->getIdField( d_elem, Record::FieldNextElem ) == 0 )
			return; // Trivialfall, keine Operation

		removeCurrentFromList();
		Record* list = getListOfElem( d_elem );
		Record* oldLast = d_txn->getRecord( 
			d_txn->getIdField( list, Record::FieldLastElm ), Record::TypeElement );
		if( oldLast == 0 )
			throw DatabaseException( DatabaseException::RecordFormat );
		d_txn->setField( oldLast, Record::FieldNextElem, DataCell().setId64( d_elem->getId() ) );
		d_txn->setField( d_elem, Record::FieldPrevElem, DataCell().setId64( oldLast->getId() ) );
		d_txn->setField( d_elem, Record::FieldNextElem, DataCell().setNull() );
		d_txn->setField( list, Record::FieldLastElm, DataCell().setId64( d_elem->getId() ) );

		UpdateInfo c;
		c.d_kind = UpdateInfo::ElementMoved;
		c.d_id = d_elem->getId();
		c.d_where = UpdateInfo::Last;
		d_txn->d_notify.append( c );
	}else
	{
		Record* list = getListOfElem( d_elem );
		if( list->getId() != d_txn->getIdField( target.d_elem, Record::FieldList) )
			throw DatabaseException( DatabaseException::WrongContext );
		Record* next = target.d_elem;
		Record* prev = d_txn->getRecord( 
			d_txn->getIdField( next, Record::FieldPrevElem ), Record::TypeElement );
		if( prev == d_elem )
			return; // Trivialfall, keine Operation

		removeCurrentFromList();
		if( prev == 0 )
		{
			// Target ist das erste Element in der Liste
			d_txn->setField( next, Record::FieldPrevElem, DataCell().setId64( d_elem->getId() ) );
			d_txn->setField( d_elem, Record::FieldNextElem, DataCell().setId64( next->getId() ) );
			d_txn->setField( d_elem, Record::FieldPrevElem, DataCell().setNull() );
			d_txn->setField( list, Record::FieldFirstElm, DataCell().setId64( d_elem->getId() ) );

			UpdateInfo c;
			c.d_kind = UpdateInfo::ElementMoved;
			c.d_id = d_elem->getId();
			c.d_where = UpdateInfo::First;
			d_txn->d_notify.append( c );
		}else
		{
			d_txn->setField( next, Record::FieldPrevElem, DataCell().setId64( d_elem->getId() ) );
			d_txn->setField( prev, Record::FieldNextElem, DataCell().setId64( d_elem->getId() ) );
			d_txn->setField( d_elem, Record::FieldPrevElem, DataCell().setId64( prev->getId() ) );
			d_txn->setField( d_elem, Record::FieldNextElem, DataCell().setId64( next->getId() ) );

			UpdateInfo c;
			c.d_kind = UpdateInfo::ElementMoved;
			c.d_id = d_elem->getId();
			c.d_id2 = next->getId();
			c.d_where = UpdateInfo::Before;
			d_txn->d_notify.append( c );
		}
	}
}

void Lit::dump()
{
	d_txn->dump( d_elem );
}
