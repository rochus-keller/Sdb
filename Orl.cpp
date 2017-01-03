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

#include "Orl.h"
#include "Record.h"
#include "Exceptions.h"
#include "Transaction.h"
#include "Database.h"
using namespace Sdb;

Orl::Orl( Record* rec, Transaction* txn )
{
	d_rec = rec;
	d_txn = txn;
	if( d_rec )
		d_rec->addRef();
}

Orl::Orl( const Orl& lhs )
{
	d_rec = 0;
	d_txn = 0;
	*this = lhs;
}

Orl::~Orl()
{
	if( d_rec )
		d_rec->release();
}

Orl& Orl::assign( const Orl& r )
{
	if( d_rec == r.d_rec )
		return *this;
	if( d_rec )
		d_rec->release();
	d_rec = r.d_rec;
	d_txn = r.d_txn;
	if( d_rec )
		d_rec->addRef();
	return *this;
}	

void Orl::checkNull() const
{
	if( d_rec == 0 )
		throw DatabaseException(DatabaseException::AccessRecord, "null");
}

void Orl::setValuePriv( const Stream::DataCell& cell, quint32 name )
{
	checkNull();
	d_txn->setField( d_rec, name, cell );
	UpdateInfo c;
	c.d_kind = UpdateInfo::ValueChanged;
	c.d_id = d_rec->getId();
	c.d_name = name;
	d_txn->d_notify.append( c );
}

void Orl::setValue( quint32 name, const Stream::DataCell& cell )
{
	if( name >= Record::MinReservedField )
		throw DatabaseException(DatabaseException::ReservedName );
	setValuePriv( cell, name );
}

void Orl::getValue( quint32 name, Stream::DataCell& v ) const
{
	checkNull();
	d_txn->getField( d_rec, name, v );
}

bool Orl::hasValue( quint32 name ) const
{
	checkNull();
	return d_txn->hasField( d_rec, name );
}

Stream::DataCell Orl::getValue( quint32 name ) const
{
	Stream::DataCell v;
	getValue( name, v );
	return v;
}

OID Orl::getId() const
{
	if( d_rec == 0 )
		return 0;
	return d_rec->getId();
}

bool Orl::isObject() const
{
	if( d_rec == 0 )
		return false;
    return d_rec->getType() == Record::TypeObject;
}

bool Orl::isDeleted() const
{
    checkNull();
    return d_rec->isDeleted();
}

bool Orl::isRelation() const
{
	if( d_rec == 0 )
		return false;
	return d_rec->getType() == Record::TypeRelation;
}

void Orl::setGuid( const QUuid& u )
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	const OID other = d_txn->derefUuid( u );
	if( other != 0 && getId() != other )
		throw DatabaseException( DatabaseException::Duplicate );
	// TODO: wie können wir verhindern, dass zwischen diesem Test und dem Eintrag jemand
	// anderst die GUID für sich beansprucht? Ev. bereits während Txn eintragen und 
	// bei Rollback allenfalls entfernen.
	Stream::DataCell cell;
	if( u.isNull() )
		cell.setNull();
	else
		cell.setUuid( u );
	setValuePriv( cell, Record::FieldUuid );
}

QUuid Orl::getGuid() const
{
	if( d_rec == 0 )
		return QUuid();
	Stream::DataCell cell;
	getValue( Record::FieldUuid, cell );
	if( !cell.isUuid() )
		return QUuid();
	return cell.getUuid();
}

Database* Orl::getDb() const
{
	checkNull();
	return d_txn->getDb();
}

quint32 Orl::getType() const
{
	if( d_rec == 0 )
		return 0;
	else
		return getValue( Record::FieldType ).getAtom();
}

void Orl::setType( quint32 t )
{
	setValuePriv( Stream::DataCell().setAtom( t ), Record::FieldType );
}

Orl::Names Orl::getNames() const
{
	checkNull();
	return d_rec->getUsedFields();
}
