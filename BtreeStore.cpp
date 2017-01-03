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

#include "BtreeStore.h"
#include "BtreeCursor.h"
#include "Exceptions.h"
#include <Sqlite3/sqlite3.h>
#include "Private.h"
#include <QBuffer>
#include <cassert>
using namespace Sdb;

static const int s_pageSize = SQLITE_DEFAULT_PAGE_SIZE;
static const int s_schema = 15;

BtreeStore::Txn::Txn( BtreeStore* db ):d_db(db)
{
	assert( db );
	d_db->transBegin();
}

void BtreeStore::Txn::rollback()
{
	if( d_db )
	{
		d_db->transAbort();
		d_db = 0;
	}
}

void BtreeStore::Txn::commit()
{
	if( d_db )
	{
		d_db->transCommit();
		d_db = 0;
	}
}

BtreeStore::Txn::~Txn()
{
	commit();
}

BtreeStore::BtreeStore( QObject* owner ):
	QObject( owner ), d_db(0), d_metaTable(0), d_txnLevel( 0)
{
}

BtreeStore::~BtreeStore()
{
	close();
}

void BtreeStore::open( const QByteArray& path )
{
	close();
	d_path = path;
	int res = sqlite3_open_v2( path, &d_db, 
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
		//| SQLITE_OPEN_EXCLUSIVE
		, 0 );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::OpenDbFile,
			::sqlite3ErrStr( res ) );

	{
		Txn lock( this );

		unsigned int tmp;
		res = sqlite3BtreeGetMeta( getBt(), s_schema, &tmp );
		if( res != SQLITE_OK )
		{
			lock.rollback();
			close();
			throw DatabaseException( DatabaseException::AccessMeta,
				::sqlite3ErrStr( res ) );
		}

		if( tmp == 0 )
		{
			d_metaTable = createTable();
			res = sqlite3BtreeUpdateMeta( getBt(), s_schema, d_metaTable );
			if( res != SQLITE_OK )
			{
				lock.rollback();
				close();
				throw DatabaseException( DatabaseException::AccessMeta,
					::sqlite3ErrStr( res ) );
			}
		}else
			d_metaTable = tmp;
	}
}

void BtreeStore::writeMeta( const QByteArray& key, const QByteArray& val )
{
	checkOpen();
	Txn lock( this );
	BtreeCursor cur;
	cur.open( this, d_metaTable, true );
	cur.insert( key, val );
}

QByteArray BtreeStore::readMeta( const QByteArray& key ) const
{
	checkOpen();
	BtreeCursor cur;
	cur.open( const_cast<BtreeStore*>(this), d_metaTable );
	if( cur.moveTo( key ) )
		return cur.readValue();
	else
		return QByteArray();
}

void BtreeStore::eraseMeta( const QByteArray& key )
{
	checkOpen();
	Txn lock( this );
	BtreeCursor cur;
	cur.open( this, d_metaTable, true );
	if( cur.moveTo( key ) )
		cur.remove();
}

void BtreeStore::close()
{
	if( d_db )
		sqlite3_close( d_db );
	d_db = 0;
	d_metaTable = 0;
}

void BtreeStore::checkOpen() const
{
	if( d_db == 0 )
		throw DatabaseException( DatabaseException::AccessDatabase, "database not open" );
}

void BtreeStore::transBegin()
{
	checkOpen();
	if( d_txnLevel == 0 )
	{
		int res = sqlite3BtreeBeginTrans( getBt(), 1 );
		if( res != SQLITE_OK )
			throw DatabaseException( DatabaseException::StartTrans, sqlite3ErrStr( res ) );
	}
	d_txnLevel++;
}

void BtreeStore::transCommit()
{
	checkOpen();
	if( d_txnLevel == 1 )
	{
		int res = sqlite3BtreeCommit( getBt() );
		if( res != SQLITE_OK )
			throw DatabaseException( DatabaseException::StartTrans, sqlite3ErrStr( res ) );
	}
	if( d_txnLevel > 0 )
		d_txnLevel--;
}

void BtreeStore::transAbort()
{
	checkOpen();
	d_txnLevel = 0; // Breche sofort ab
	sqlite3BtreeRollback( getBt() );
}

int BtreeStore::createTable(bool noData)
{
	checkOpen();
	int table;
	Txn lock( this );
	int res = sqlite3BtreeCreateTable( getBt(), &table, (noData)? BTREE_ZERODATA:0 );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::CreateTable, sqlite3ErrStr( res ) );
	return table;
}

void BtreeStore::dropTable( int table )
{
	checkOpen();
	Txn lock( this );
	int res = sqlite3BtreeDropTable( getBt(), table, 0 );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::RemoveTable, sqlite3ErrStr( res ) );
}

void BtreeStore::clearTable( int table )
{
	checkOpen();
	Txn lock( this );
	int res = sqlite3BtreeClearTable( getBt(), table);
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::ClearTable, sqlite3ErrStr( res ) );
}

Btree* BtreeStore::getBt() const
{
	if( d_db == 0 )
		return 0;
	return d_db->aDb->pBt;
}

