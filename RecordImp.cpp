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

#include "RecordImp.h"
#include "Exceptions.h"
#include "Database.h"
#include <Stream/DataWriter.h>
#include <Stream/DataReader.h>
#include <Stream/Helper.h>
#include <QIODevice>
#include <cassert>
#include <QtDebug>
using namespace Sdb;
using namespace Stream;

RecordImp::RecordImp(Database* db, OID id, Record::Type t )
{
	d_state = StateIdle;
	d_id = id;
	d_type = t;
	d_cow = 0;
	d_db = db;
	d_refCount = 0;
	d_cow = 0;
}

void RecordImp::writeTo( QIODevice* out ) const
{
	assert( out != 0 );
	DataWriter w( out );

	// Version 1
	w.writeSlot( DataCell().setUInt8(1) );
	w.writeSlot( DataCell().setUInt8(d_type) );

	// Fixe Felder
	switch( d_type )
	{
	case TypeObject:
		Helper::writeMultibyte64( out, d_fields.value(FieldOwner).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldPrevObj).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldNextObj).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldFirstObj).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldLastObj).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldFirstRel).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldLastRel).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldFirstElm).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldLastElm).toId64() );
		break;
	case TypeRelation:
		Helper::writeMultibyte64( out, d_fields.value(FieldSource).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldTarget).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldPrevSource).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldNextSource).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldPrevTarget).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldNextTarget).toId64() );
		break;
	case TypeElement:
		Helper::writeMultibyte64( out, d_fields.value(FieldList).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldPrevElem).toId64() );
		Helper::writeMultibyte64( out, d_fields.value(FieldNextElem).toId64() );
		break;
	}
	w.startFrame();
	Record::Fields::const_iterator i;
	for( i = d_fields.begin(); i != d_fields.end(); ++i )
	{
		if( ( i.key() < MinReservedField ||
			  i.key() == Record::FieldValue ||
			  i.key() == Record::FieldType ||
			  i.key() == Record::FieldUuid ) && i.value().hasValue() )
			w.writeSlot( i.value(), i.key(), true ); // RISK: Komprimiert Speichern
	}
	w.endFrame();
}

static void _readMb( QIODevice* in, Record::Fields& fields, quint32 a, DataCell::DataType t )
{
	OID i;
	if( Helper::readMultibyte64( in, i ) < 0 )
		throw DatabaseException( DatabaseException::RecordFormat );
	if( i == 0 )
		return;
	switch( t )
	{
	case DataCell::TypeOid:
		fields[a].setOid( i );
		break;
	case DataCell::TypeRid:
		fields[a].setRid( i );
		break;
	case DataCell::TypeId64:
		fields[a].setId64( i );
		break;
	default:
		assert( false );
	}
}

void RecordImp::readFrom( QIODevice* in )
{
	assert( in != 0 );
	DataReader r( in );

	clear();

	DataCell v;
	DataReader::Token t = r.nextToken();
	if( t != DataReader::Slot )
		throw DatabaseException( DatabaseException::RecordFormat );
	r.readValue( v );
	if( v.getType() != DataCell::TypeUInt8 || v.getUInt8() != 1 )
		throw DatabaseException( DatabaseException::RecordFormat, "wrong version" );
	t = r.nextToken();
	if( t != DataReader::Slot )
		throw DatabaseException( DatabaseException::RecordFormat );
	r.readValue( v );
	//qDebug() << "type=" << v.getType() << " val=" << v.getUInt8();
	if( v.getType() != DataCell::TypeUInt8 )
		throw DatabaseException( DatabaseException::RecordFormat );
	d_type = v.getUInt8();
	switch( d_type )
	{
	case TypeObject:
		_readMb( in, d_fields, FieldOwner, DataCell::TypeOid );
		_readMb( in, d_fields, FieldPrevObj, DataCell::TypeOid );
		_readMb( in, d_fields, FieldNextObj, DataCell::TypeOid );
		_readMb( in, d_fields, FieldFirstObj, DataCell::TypeOid );
		_readMb( in, d_fields, FieldLastObj, DataCell::TypeOid );
		_readMb( in, d_fields, FieldFirstRel, DataCell::TypeRid );
		_readMb( in, d_fields, FieldLastRel, DataCell::TypeRid );
		_readMb( in, d_fields, FieldFirstElm, DataCell::TypeId64 );
		_readMb( in, d_fields, FieldLastElm, DataCell::TypeId64 );
		break;
	case TypeRelation:
		_readMb( in, d_fields, FieldSource, DataCell::TypeOid );
		_readMb( in, d_fields, FieldTarget, DataCell::TypeOid );
		_readMb( in, d_fields, FieldPrevSource, DataCell::TypeRid );
		_readMb( in, d_fields, FieldNextSource, DataCell::TypeRid );
		_readMb( in, d_fields, FieldPrevTarget, DataCell::TypeRid );
		_readMb( in, d_fields, FieldNextTarget, DataCell::TypeRid );
		break;
	case TypeElement:
		_readMb( in, d_fields, FieldList, DataCell::TypeOid );
		_readMb( in, d_fields, FieldPrevElem, DataCell::TypeId64 );
		_readMb( in, d_fields, FieldNextElem, DataCell::TypeId64 );
		break;
	default:
		throw DatabaseException( DatabaseException::RecordFormat );
	}
	t = r.nextToken();
	if( t != DataReader::BeginFrame )
		throw DatabaseException( DatabaseException::RecordFormat );
	t = r.nextToken();
	while( t == DataReader::Slot )
	{
		r.readValue( v );
		if( r.getName().getType() != DataCell::TypeAtom )
			throw DatabaseException( DatabaseException::RecordFormat );
		d_fields[r.getName().getAtom()] = v;
		t = r.nextToken();
	}
	if( t != DataReader::EndFrame )
		throw DatabaseException( DatabaseException::RecordFormat );
}

void RecordImp::clear()
{
	d_type = TypeUndefined;
	d_state = StateIdle;
	d_fields.clear();
}

const Stream::DataCell& RecordImp::getField( quint32 id ) const
{
	// Solange StateToDelete dürfen die Werte noch gelesen werden.
	if( d_state == StateDeleted )
		throw DatabaseException( DatabaseException::RecordDeleted );
	Record::Fields::const_iterator i = d_fields.find( id );
	if( i == d_fields.end() )
		return Record::getNull();
	else
		return i.value();
}

void RecordImp::addRef() 
{ 
#if QT_VERSION >= 0x040400
	d_refCount++; // TODO
#else
	q_atomic_increment( &d_refCount );
#endif
}

void RecordImp::release() 
{ 
#if QT_VERSION >= 0x040400
	d_refCount--; // TODO
#else
	q_atomic_decrement( &d_refCount );
#endif
	if( d_refCount <= 0 )
		d_db->checkUsed( d_id );
}

void RecordImp::dump()
{
	qDebug() << "****Record id=" << d_id << " type=" << d_type;
	Record::Fields::const_iterator i;
	for( i = d_fields.begin(); i != d_fields.end(); ++i )
	{
		qDebug() << QString("key=atom(0x%1)").arg( i.key(), 0, 16 ) << " value=" << i.value().toPrettyString();
	}
	qDebug() << "++++End Record";
}

RecordImp::UsedFields RecordImp::getUsedFields() const
{
	UsedFields res;
	Record::Fields::const_iterator i;
	for( i = d_fields.begin(); i != d_fields.end(); ++i )
		if( i.key() < MinReservedField )
			res.insert( i.key() );
	return res;
}
