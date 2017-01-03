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

#include "Idx.h"
#include "Exceptions.h"
#include "BtreeStore.h"
#include "BtreeCursor.h"
#include "Transaction.h"
#include <cassert>
using namespace Sdb;
using namespace Stream;

Idx::Idx( Transaction* txn, int idx )
{
	d_txn = txn;
	d_idx = idx;
}

Idx::Idx( const Idx& lhs )
{
	d_txn = 0;
	d_idx = 0;
	*this = lhs;
}

Idx& Idx::operator=( const Idx& r )
{
	if( d_idx == r.d_idx )
		return *this;
	d_txn = r.d_txn;
	d_idx = r.d_idx;
	d_cur = r.d_cur;
	d_key = r.d_key;
	return *this;
}	

void Idx::checkNull() const
{
	if( d_idx == 0 )
		throw DatabaseException(DatabaseException::AccessRecord, "null");
}

bool Idx::first()
{
	checkNull();
	Database::Lock lock( d_txn->getDb(), false );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_idx );
	if( cur.moveFirst() )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

bool Idx::last()
{
	checkNull();
	Database::Lock lock( d_txn->getDb(), false );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_idx );
	if( cur.moveLast() )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

bool Idx::next()
{
	checkNull();
	Database::Lock lock( d_txn->getDb(), false );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_idx );
	cur.moveTo( d_cur ); // zur letztbekannten oder neu verlangten Position
	if( cur.moveNext() )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

bool Idx::nextKey()
{
	if( next() )
		return d_cur.startsWith( d_key );
	else
		return false;
}

bool Idx::prevKey()
{
	if( prev() )
		return d_cur.startsWith( d_key );
	else
		return false;
}

OID Idx::getId()
{
	checkNull();
	Database::Lock lock( d_txn->getDb(), false );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_idx );
	if( !cur.moveTo( d_cur ) )
		return 0; // zur letztbekannten oder neu verlangten Position
	DataCell id;
	id.readCell( cur.readValue() );
	return id.getId64();
}

bool Idx::prev()
{
	checkNull();
	Database::Lock lock( d_txn->getDb(), false );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_idx );
	cur.moveTo( d_cur ); // zur letztbekannten oder neu verlangten Position
	if( cur.movePrev() )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

bool Idx::seek( const Stream::DataCell& key )
{
	checkNull();
	Database::Lock lock( d_txn->getDb(), false );
	d_key.clear();
	d_cur.clear();
	IndexMeta meta;
	d_txn->getDb()->getIndexMeta( d_idx, meta );
	assert( !meta.d_items.isEmpty() );
	addElement( d_key, meta.d_items[0], key );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_idx );
	if( cur.moveTo( d_key, true ) )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

bool Idx::firstKey()
{
	checkNull();
	Database::Lock lock( d_txn->getDb(), false );
	d_cur.clear();
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_idx );
	if( cur.moveTo( d_key, true ) )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

bool Idx::gotoCur( const QByteArray& cur )
{
	if( cur.startsWith( d_key ) )
	{
		d_cur = cur;
		return true;
	}else
		return false;
}

bool Idx::seek( const QList<Stream::DataCell>& keys )
{
	checkNull();
	Database::Lock lock( d_txn->getDb(), false );
	d_key.clear();
	d_cur.clear();
	IndexMeta meta;
	d_txn->getDb()->getIndexMeta( d_idx, meta );
	for( int i = 0; i < keys.size() && i < meta.d_items.size(); i++ )
		addElement( d_key, meta.d_items[i], keys[i] );
	// TODO: was ist, wenn size von keys und meta.items nicht gleich?
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_idx );
	if( cur.moveTo( d_key, true ) )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

void Idx::addElement( QByteArray& out, const IndexMeta::Item& i, const Stream::DataCell& v )
{
	QByteArray cell;
	DataCell::DataType t = v.getType();
	switch( v.getType() )
	{
	case DataCell::TypeLatin1:
		if( i.d_nocase )
			collate( cell, i.d_coll, QString::fromLatin1( v.getArr() ).toLower() );
		else
			collate( cell, i.d_coll, QString::fromLatin1( v.getArr() ) );
		t = DataCell::TypeString; // Alle Textidx als UTF-8 speichern
		break;
	case DataCell::TypeAscii:
		if( i.d_nocase )
			cell = v.getArr().toLower();
		else
			cell = v.getArr();
		t = DataCell::TypeString; // Alle Textidx als UTF-8 speichern
		break;
	case DataCell::TypeString:
		if( i.d_nocase )
			collate( cell, i.d_coll, v.getStr().toLower() );
		else
			collate( cell, i.d_coll, v.getStr() );
		break;
	default:
		// Alle �brigen Typen inkl. TypeHtml etc.
		cell = v.writeCell(true);
		break;
	}
	if( i.d_invert )
	{
		for( int i = 0; i < cell.size(); i++ )
			cell[i] = ~cell[i];
	}
	out += DataCell::typeToSym( t ); // Damit Datentypen nicht durcheinander sortiert
	out += cell;
}

void Idx::collate( QByteArray& out, quint8 c, const QString& in )
{
	if( c == IndexMeta::None )
		out = in.toUtf8();
	else if( c == IndexMeta::NFKD_CanonicalBase )
	{
		out.reserve( in.size() * 1.5 );
		char c;
		for( int i = 0; i < in.size(); i++ )
		{
			switch( in[i].decompositionTag() )
			{
			case QChar::NoDecomposition:
				c = in[i].toAscii();
				if( c & 0x80 )// > 127
					out += QString( c ).toUtf8();
				else
					out += c;
				break;
			case QChar::Canonical:
				c = in[i].decomposition()[0].toAscii();
				if( c & 0x80 )// > 127
					out += QString( c ).toUtf8();
				else
					out += c;
				break;
			default:
				out += in[i].decomposition().toUtf8();
				break;
			}
		}
	}else
		qWarning( "Idx::collate: unknown Collation" );
}
