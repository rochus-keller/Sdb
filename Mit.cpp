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

#include "Mit.h"
#include "Record.h"
#include "Exceptions.h"
#include "Transaction.h"
#include "Database.h"
#include <Sdb/BtreeCursor.h>
#include <Stream/DataCell.h>
#include <Stream/BmlRecord.h>
using namespace Sdb;
using namespace Stream;

Mit::Mit( Record* rec, Transaction* txn )
{
	d_rec = rec;
	d_txn = txn;
	if( d_rec )
		d_rec->addRef();
}

Mit::Mit( const Mit& lhs )
{
	d_rec = 0;
	d_txn = 0;
	d_cur.clear();
	d_key.clear();
	*this = lhs;
}

bool Mit::seek( const QList<Stream::DataCell>& key )
{
	// RISK: sucht nur in Db, nicht in Transaktion
	checkNull();
	Database::Lock lock( d_txn->getDb(), false );
	d_key.clear();
	d_cur.clear();
	DataWriter w;
	w.writeSlot( DataCell().setOid( d_rec->getId() ) );
	for( int i = 0; i < key.size(); i++ )
		w.writeSlot( key[i] );
	d_key = w.getStream();

	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getMapTable() );
	if( cur.moveTo( d_key, true ) )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

Mit::~Mit()
{
	if( d_rec )
		d_rec->release();
}

Mit& Mit::assign( const Mit& r )
{
	if( d_rec == r.d_rec && d_cur == r.d_cur && d_key == r.d_key )
		return *this;
	if( d_rec )
		d_rec->release();
	d_rec = r.d_rec;
	d_txn = r.d_txn;
	d_cur = r.d_cur;
	d_key = r.d_key;
	if( d_rec )
		d_rec->addRef();
	return *this;
}	

void Mit::checkNull() const
{
	if( d_rec == 0 )
		throw DatabaseException(DatabaseException::AccessRecord, "null");
}

void Mit::getValue( Stream::DataCell& v ) const
{
	checkNull();
	v.setNull();
	if( !d_cur.startsWith( d_key ) )
		return;

	Database::Lock lock( d_txn->getDb(), false );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getMapTable(), false );
	if( cur.moveTo( d_cur ) )
	{
		v.readCell( cur.readValue() );
	}
}

Stream::DataCell Mit::getValue() const
{
	Stream::DataCell v;
	getValue( v );
	return v;
}

QList<Stream::DataCell> Mit::getKey() const
{
	QList<Stream::DataCell> res;
	if( !d_cur.startsWith( d_key ) )
		return res;
	DataReader r( d_cur );
	DataReader::Token t = r.nextToken();
	bool first = true;
	while( t == DataReader::Slot )
	{
		Stream::DataCell v;
		r.readValue( v );
		if( !first )
			res.append( v ); // erster ist Oid
		first = false;
		t = r.nextToken();
	}
	return res;
}

bool Mit::firstKey()
{
	checkNull();
	Database::Lock lock( d_txn->getDb(), false );
	d_cur.clear();
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getMapTable() );
	if( cur.moveTo( d_key, true ) )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

bool Mit::nextKey()
{
	checkNull();
	Database::Lock lock( d_txn->getDb(), false );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getMapTable() );
	cur.moveTo( d_cur ); // zur letztbekannten oder neu verlangten Position
	if( cur.moveNext() )
	{
		d_cur = cur.readKey();
		return d_cur.startsWith( d_key );
	}else
		return false;
}

bool Mit::prevKey()
{
	checkNull();
	Database::Lock lock( d_txn->getDb(), false );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getMapTable() );
	cur.moveTo( d_cur ); // zur letztbekannten oder neu verlangten Position
	if( cur.movePrev() )
	{
		d_cur = cur.readKey();
		return d_cur.startsWith( d_key );
	}else
		return false;
}
