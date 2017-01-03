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

#include "Database.h"
#include "Database.h"
#include "BtreeCursor.h"
#include "BtreeStore.h"
#include "Exceptions.h"
#include "RecordImp.h"
#include "Idx.h"
#include <Stream/DataCell.h>
#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>
#include <QBuffer>
#include <QtDebug>
#include <QFileInfo>
#include <QDir>
#include <cassert>
using namespace Sdb;
using namespace Stream;

static const char* s_streams = ".streams";

Database::Lock::Lock( Database* db, bool txn ):d_db(db), d_txn(txn)
{
	assert( db ); 
	//d_db->d_lock.lock();
	if( d_txn )
		d_db->d_db->transBegin();
}

void Database::Lock::rollback()
{
	if( d_db )
	{
		if( d_txn )
			d_db->d_db->transAbort();
		//d_db->d_lock.unlock();
		d_db = 0;
	}
}

void Database::Lock::commit()
{
	if( d_db )
	{
		if( d_txn )
			d_db->d_db->transCommit();
		//d_db->d_lock.unlock();
		d_db = 0;
	}
}

Database::Lock::~Lock()
{
	commit();
}

Database::Database(QObject*p):QObject(p)//,d_lock(QMutex::Recursive)
{
	d_db = 0;
	qRegisterMetaType<Sdb::UpdateInfo>();
	connect( this, SIGNAL(doCheckUsed( quint64 )),
		this,SLOT(onCheckUsed( quint64 )),Qt::QueuedConnection );
}

Database::~Database()
{
	close();
}

void Database::addObserver( QObject* obj, const char* slot )
{
	connect( this, SIGNAL(notify( Sdb::UpdateInfo )),
		obj, slot, Qt::QueuedConnection );
}

void Database::removeObserver( QObject* obj, const char* slot )
{
	disconnect( this, SIGNAL(notify( Sdb::UpdateInfo )),obj, slot );
}

void Database::open( const QString& path )
{
	close();
	d_db = new BtreeStore( this );
	d_db->open( path.toUtf8() );
	loadMeta();
}

void Database::close()
{
	emit notify( UpdateInfo( UpdateInfo::DbClosing ) );
	if( d_db )
		delete d_db;
	d_db = 0;
	d_meta = Meta();
	// TODO: d_cache + Records löschen
}

RecordImp* Database::getOrLoadRecord( OID id )
{
	RecordImp* r = d_cache.value( id );
	if( r != 0 )
	{
		if( r->isDeleted() )
			return 0;
		else
			return r;
	}
	BtreeCursor cur;
	cur.open( d_db, getObjTable() );
	if( !cur.moveTo( DataCell().setId64( id ).writeCell() ) )
	{
		// Record existiert noch nicht. 
		return 0;
	}else
	{
		// Record existiert in Db. Lade ihn.
		QBuffer buf;
		buf.buffer() = cur.readValue();
		buf.open( QIODevice::ReadOnly );
		r = new RecordImp( this, id, Record::TypeUndefined );
		try
		{
			r->readFrom( &buf );
		}catch( std::exception& )
		{
			delete r;
			r = 0;
			throw;
		}
	}
	d_cache[id] = r;
	return r;
}

void Database::checkUsed( OID id )
{
	emit doCheckUsed( id );
}

void Database::onCheckUsed( OID id )
{
	Lock lock( this, false );
	QHash<OID,RecordImp*>::iterator i = d_cache.find( id );
	if( i != d_cache.end() )
	{
		if( i.value()->getRefCount() <= 0 )
		{
			delete i.value();
			d_cache.erase( i );
		}
	}
}

void Database::saveRecord( RecordImp* r )
{
	assert( r );
	BtreeCursor cur;
	cur.open( d_db, getObjTable(), true );
	QBuffer buf;
	buf.open( QIODevice::WriteOnly );
	r->writeTo( &buf );
	buf.close();
	cur.insert( DataCell().setId64( r->getId() ).writeCell(), buf.buffer() );
}

void Database::eraseRecord( RecordImp* r )
{
	assert( r );
	BtreeCursor cur;
	cur.open( d_db, getObjTable(), true );
	const bool res = cur.moveTo( DataCell().setId64( r->getId() ).writeCell() );
	assert( res );
	cur.remove();
}

void Database::dumpQueue( OID id )
{
	BtreeCursor cur;
	cur.open( d_db, getQueTable(), false );
	if( cur.moveFirst() ) do
	{
		QString str;
		DataReader r( cur.readKey() );
		DataReader::Token t = r.nextToken();
		while( DataReader::isUseful( t ) )
		{
			str += r.readValue().toPrettyString();
			str += " ";
			t = r.nextToken();
		}
		str += " = ";
		DataCell v;
		v.readCell( cur.readValue() );
		str += v.toPrettyString();
		qDebug() << str;
	}while( cur.moveNext() );
}

void Database::dump()
{
	BtreeCursor cur;
	cur.open( d_db, getObjTable(), false );
	if( cur.moveFirst() ) do
	{
		DataCell k;
		k.readCell( cur.readKey() );
		qDebug() << "Key=" << k.toPrettyString();
		if( k.isNull() || k.isUuid() )
		{
			DataCell v;
			v.readCell( cur.readValue() );
			qDebug() << "Value=" << v.toPrettyString();
		}else
		{
			RecordImp r(this, k.getId64() );
			QBuffer buf;
			buf.buffer() = cur.readValue();
			buf.open( QIODevice::ReadOnly );
			r.readFrom( &buf );
			r.dump();
		}
    }while( cur.moveNext() );
}

void Database::dumpAtoms()
{
    BtreeCursor cur;
    cur.open( d_db, getDirTable(), false );
    if( cur.moveFirst() ) do
	{
		DataCell k;
		k.readCell( cur.readKey() );
		qDebug() << "Key=" << k.toPrettyString() << cur.readKey().toHex();
        DataCell atom;
        atom.readCell( cur.readValue() );
        qDebug() << "Value=" << atom.toPrettyString();
    }while( cur.moveNext() );
}

void Database::loadMeta()
{
	// Header ist vorhanden
	DataReader reader( d_db->readMeta( DataCell().setNull().writeCell() ) );
	d_meta = Meta();

	for( DataReader::Token t = reader.nextToken(); DataReader::isUseful( t ); t = reader.nextToken() )
	{
		switch( t )
		{
		case DataReader::Slot:
			{
				const QByteArray name = reader.getName().getArr();
				DataCell value;
				reader.readValue( value );
				if( name == "objTable" )
					d_meta.d_objTable = value.getInt32();
				else if( name == "strTable" )
					d_meta.d_strTable = value.getInt32();
				else if( name == "idxTable" )
					d_meta.d_idxTable = value.getInt32();
				else if( name == "dirTable" )
					d_meta.d_dirTable = value.getInt32();
				else if( name == "queTable" )
					d_meta.d_queTable = value.getInt32();
				else if( name == "mapTable" )
					d_meta.d_mapTable = value.getInt32();
				// else
					// throw DatabaseException( DatabaseException::DatabaseMeta, "invalid meta header format" );
					// stattdessen ignorieren
			}
			break;
		default:
			throw DatabaseException( DatabaseException::DatabaseMeta, 
				"invalid meta header format" );
		}
	}
}

void Database::saveMeta()
{
	DataWriter value;
	value.writeSlot( DataCell().setInt32( d_meta.d_objTable ), "objTable" );
	value.writeSlot( DataCell().setInt32( d_meta.d_dirTable ), "dirTable" );
	value.writeSlot( DataCell().setInt32( d_meta.d_strTable ), "strTable" );
	value.writeSlot( DataCell().setInt32( d_meta.d_idxTable ), "idxTable" );
	value.writeSlot( DataCell().setInt32( d_meta.d_queTable ), "queTable" );
	if( d_meta.d_mapTable )
		value.writeSlot( DataCell().setInt32( d_meta.d_mapTable ), "mapTable" );
	d_db->writeMeta( DataCell().setNull().writeCell(), value.getStream() );
}

void Database::checkOpen() const
{
	if( d_db == 0 )
		throw DatabaseException( DatabaseException::AccessDatabase, "database not open" );
}

int Database::getObjTable()
{
	checkOpen();
	if( d_meta.d_objTable == 0 )
	{
		BtreeStore::Txn txn( d_db );
		d_meta.d_objTable = d_db->createTable();
		saveMeta();
	}
	return d_meta.d_objTable;
}

int Database::getStrTable()
{
	checkOpen();
	if( d_meta.d_strTable == 0 )
	{
		BtreeStore::Txn txn( d_db );
		d_meta.d_strTable = d_db->createTable();
		saveMeta();
	}
	return d_meta.d_strTable;
}

int Database::getDirTable()
{
	checkOpen();
	if( d_meta.d_dirTable == 0 )
	{
		BtreeStore::Txn txn( d_db );
		d_meta.d_dirTable = d_db->createTable();
		saveMeta();
	}
	return d_meta.d_dirTable;
}

int Database::getIdxTable()
{
	checkOpen();
	if( d_meta.d_idxTable == 0 )
	{
		BtreeStore::Txn txn( d_db );
		d_meta.d_idxTable = d_db->createTable();
		saveMeta();
	}
	return d_meta.d_idxTable;
}

int Database::getQueTable()
{
	checkOpen();
	if( d_meta.d_queTable == 0 )
	{
		BtreeStore::Txn txn( d_db );
		d_meta.d_queTable = d_db->createTable();
		saveMeta();
	}
	return d_meta.d_queTable;
}

int Database::getMapTable()
{
	checkOpen();
	if( d_meta.d_mapTable == 0 )
	{
		BtreeStore::Txn txn( d_db );
		d_meta.d_mapTable = d_db->createTable();
		saveMeta();
	}
	return d_meta.d_mapTable;
}

OID Database::getNextOid(bool persist)
{
	checkOpen();
	OID id = 0;
	BtreeStore::Txn txn( d_db );
	BtreeCursor cur;

	cur.open( d_db, getObjTable(), true );
	if( cur.moveTo( DataCell().setNull().writeCell() ) )
	{
		DataCell v;
		v.readCell( cur.readValue() );
		id = v.toId64();
	}
	id++;
	if( persist )
		cur.insert( DataCell().setNull().writeCell(), DataCell().setUInt64( id ).writeCell() );
	return id;
}

quint32 Database::getNextSid()
{
	checkOpen();
	quint32 id = 0;
	BtreeStore::Txn txn( d_db );
	BtreeCursor cur;
	cur.open( d_db, getStrTable(), true );
	if( cur.moveTo( DataCell().setNull().writeCell() ) )
	{
		DataCell v;
		v.readCell( cur.readValue() );
		id = v.getSid();
	}
	id++;
	cur.insert( DataCell().setNull().writeCell(), DataCell().setSid( id ).writeCell() );
	return id;
}

quint32 Database::getNextQueueNr( OID oid )
{
	checkOpen();
	quint32 id = 0;
	BtreeStore::Txn txn( d_db );
	BtreeCursor cur;
	cur.open( d_db, getQueTable(), true );
	const QByteArray key = DataCell().setId64(oid).writeCell();
	if( cur.moveTo( key ) )
	{
		DataCell v;
		v.readCell( cur.readValue() );
		id = v.getId32();
	}
	id++;
	cur.insert( key, DataCell().setId32( id ).writeCell() );
	return id;
}

QByteArray Database::getAtomString( quint32 a )
{
	if( a == 0 )
		return QByteArray();
	Lock lock( this, false );
	QHash<quint32,QByteArray>::const_iterator i = d_invDir.find( a );
	if( i != d_invDir.end() )
		return i.value();
	BtreeCursor cur;
	cur.open( d_db, getDirTable(), false );
	if( cur.moveTo( DataCell().setAtom( a ).writeCell() ) )
	{
		DataCell s;
		s.readCell( cur.readValue() );
		d_dir[s.getArr()] = a;
		d_invDir[a] = s.getArr();
		return s.getArr();
	}else
		return QByteArray();
}

quint32 Database::getAtom( const QByteArray& name, bool create )
{
	Lock lock( this, false );
	QHash<QByteArray,quint32>::const_iterator i = d_dir.find( name );
	if( i != d_dir.end() )
		return i.value();
	const QByteArray n = DataCell().setLatin1(name).writeCell();
	// Suche Atom in Db
	{
		BtreeCursor cur;
		cur.open( d_db, getDirTable(), false );
        // partial match würde eh nichts nützen, da Anzahl mitverglichen wird.
		if( cur.moveTo( n ) )
		{
			DataCell atom;
			atom.readCell( cur.readValue() );
			if( atom.getType() != DataCell::TypeAtom )
				throw DatabaseException( DatabaseException::RecordFormat );
			d_dir[name] = atom.getAtom();
			d_invDir[atom.getAtom()] = name;
			return atom.getAtom();
		}
	}
	// Atom existiert noch nicht, erzeuge es
    if( create )
	{
		BtreeStore::Txn txn( d_db );
		BtreeCursor cur;
		cur.open( d_db, getDirTable(), true );
		quint32 atom = 0;
		const QByteArray null = DataCell().setNull().writeCell();
		if( cur.moveTo( null ) )
		{
			DataCell v;
			v.readCell( cur.readValue() );
			if( v.getType() != DataCell::TypeAtom )
				throw DatabaseException( DatabaseException::RecordFormat );
			atom = v.getAtom();
		}
		atom++;
		const QByteArray a = DataCell().setAtom( atom ).writeCell();
		cur.insert( null, a );
		cur.insert( n, a );
		cur.insert( a, n );
		d_dir[name] = atom;
		d_invDir[atom] = name; // name wird nur einmal gespeichert
		return atom;
	}else
        return 0;
}

void Database::presetAtom( const QByteArray& name, quint32 atom )
{
	checkOpen();
	// Suche den Wert im Cache und in der DB
	Lock lock( this, false );
	QHash<QByteArray,quint32>::const_iterator i = d_dir.find( name );
	if( i != d_dir.end() )
	{
		// Name ist bereits im Cache. Prüfe nun ob ID übereinstimmt.
		if( i.value() != atom )
			throw DatabaseException( DatabaseException::AtomClash );
		// Name wurde bereits im Cache registriert mit gewünschter Atom-ID
		return; 
	}
	const QByteArray n = DataCell().setLatin1(name).writeCell();
	QByteArray a;
	// Suche Atom in Db
	BtreeCursor cur;
	cur.open( d_db, getDirTable(), false );
	if( cur.moveTo( n ) )
	{
		a = cur.readValue();
		DataCell v;
		v.readCell( a );
		if( v.getType() != DataCell::TypeAtom )
			throw DatabaseException( DatabaseException::RecordFormat );
		if( v.getAtom() != atom )
			throw DatabaseException( DatabaseException::AtomClash );
		// Prüfe nun, ob Atom-ID bereits in der DB vorhanden. 
		if( cur.moveTo( a ) )
		{
			if( cur.readValue() != n )
				// Darf in einer korrekten DB nicht vorkommen.
				throw DatabaseException( DatabaseException::RecordFormat );
		}
		// Name ist in Db vorhanden mit gewünschter Atom-ID
		return;
	}else
		a = DataCell().setAtom( atom ).writeCell();
	cur.close();
	// Füge Atom in Db ein
	{
		BtreeStore::Txn txn( d_db );
		cur.open( d_db, getDirTable(), true );
		const QByteArray null = DataCell().setNull().writeCell();
		if( cur.moveTo( null ) )
		{
			DataCell v;
			v.readCell( cur.readValue() );
			if( v.getType() != DataCell::TypeAtom )
				throw DatabaseException( DatabaseException::RecordFormat );
			if( atom > v.getAtom() )
				// Erhöhe den Zähler auf mindestens die Preset-ID
				cur.insert( null, a );
		}else
			// Zähler war noch nicht vorhanden
			cur.insert( null, a );
		cur.insert( n, a );
		cur.insert( a, n );
	}
}

QString Database::getStreamsDir() const
{
	checkOpen();
	QFileInfo info = QString::fromLatin1( d_db->getPath() );
	QDir path = info.absoluteDir();
	if( !path.exists( info.baseName() + s_streams ) ) 
	{
		if( !path.mkdir( info.baseName() + s_streams ) ) 
			throw DatabaseException( DatabaseException::StreamsDir, "cannot create" );
	}
	return path.filePath( info.baseName() + s_streams );
}

bool Database::lockStream( quint32 id, bool write )
{
	QHash<quint32,int>::iterator i = d_streamLocks.find( id );
	if( i == d_streamLocks.end() )
	{
		d_streamLocks[id] = (write)?-1:+1;
	}else
	{
		if( i.value() < 0 )
			return false;
		else
			i.value()++;
	}
	StreamMeta meta;
	loadStreamMeta( id, meta );
	meta.d_lastUse = QDateTime::currentDateTime();
	meta.d_useCount++;
	saveStreamMeta( id, meta );
	return true;
}

bool Database::unlockStream( quint32 id )
{
	QHash<quint32,int>::iterator i = d_streamLocks.find( id );
	if( i == d_streamLocks.end() )
		return false;
	if( i.value() < 0 )
	{
		d_streamLocks.remove( id );
		return true;
	}else
	{
		i.value()--;
		if( i.value() == 0 )
			d_streamLocks.remove( id );
		return true;
	}
}

bool Database::isStreamWriteLocked( quint32 id ) const
{
	QHash<quint32,int>::const_iterator i = d_streamLocks.find( id );
	if( i == d_streamLocks.end() )
		return false;
	else
		return i.value() < 0;
}

void Database::saveStreamMeta( quint32 id, const StreamMeta& meta )
{
	checkOpen();
	BtreeStore::Txn txn( d_db );
	DataWriter value;
	value.writeSlot( DataCell().setAscii( meta.d_encoding ), NameTag("enc") );
	value.writeSlot( DataCell().setAscii( meta.d_mimeType ), NameTag("mime") );
	value.writeSlot( DataCell().setAscii( meta.d_suffix ), NameTag("suff") );
	value.writeSlot( DataCell().setAscii( meta.d_locale ), NameTag("loc") );
	if( meta.d_lastUse.isValid() )
		value.writeSlot( DataCell().setDateTime( meta.d_lastUse ), NameTag("lu") );
	value.writeSlot( DataCell().setUInt32( meta.d_useCount ), NameTag("uc") );
	value.writeSlot( DataCell().setBool( meta.d_gzipped ), NameTag("gzip") );
	value.writeSlot( DataCell().setBool( meta.d_crypted ), NameTag("cryp") );
	BtreeCursor cur;
	cur.open( d_db, getStrTable(), true );
	cur.insert( DataCell().setSid( id ).writeCell(), value.getStream() );
}

bool Database::loadStreamMeta( quint32 id, StreamMeta& meta )
{
	BtreeCursor cur;
	cur.open( d_db, getStrTable() );
	if( !cur.moveTo( DataCell().setSid( id ).writeCell() ) )
	{
		// Record existiert noch nicht. 
		return false;
	}else
	{
		// Record existiert in Db. Lade ihn.
		DataReader reader( cur.readValue() );
		for( DataReader::Token t = reader.nextToken(); DataReader::isUseful( t ); 
			t = reader.nextToken() )
		{
			switch( t )
			{
			case DataReader::Slot:
				{
					const NameTag name = reader.getName().getTag();
					DataCell value;
					reader.readValue( value );
					if( name == "enc" )
						meta.d_encoding = value.getArr();
					else if( name == "mime" )
						meta.d_mimeType = value.getArr();
					else if( name == "suff" )
						meta.d_suffix = value.getArr();
					else if( name == "loc" )
						meta.d_locale = value.getArr();
					else if( name == "lu" )
						meta.d_lastUse = value.getDateTime();
					else if( name == "uc" )
						meta.d_useCount = value.getUInt32();
					else if( name == "gzip" )
						meta.d_gzipped = value.getBool();
					else if( name == "cryp" )
						meta.d_crypted = value.getBool();
				}
				break;
			default:
				throw DatabaseException( DatabaseException::DatabaseMeta, 
					"invalid stream meta format" );
			}
		}
	}
	return true;
}

quint32 Database::findIndex( const QByteArray& name )
{
	checkOpen();
	Lock lock( this, false );
	BtreeCursor cur;
	cur.open( d_db, getIdxTable(), false );
	if( cur.moveTo( DataCell().setLatin1( name ).writeCell() ) )
	{
		DataCell id;
		id.readCell( cur.readValue() );
		return id.getId32();
	}else
		return 0;
}

static QByteArray writeIndexMeta( const IndexMeta& m )
{
	DataWriter w;
	w.writeSlot( DataCell().setUInt8( m.d_kind ), NameTag("kind") );
	for( int i = 0; i < m.d_items.size(); i++ )
	{
		w.startFrame( NameTag("item") );
		w.writeSlot( DataCell().setAtom( m.d_items[i].d_atom ), NameTag("atom") );
		w.writeSlot( DataCell().setBool( m.d_items[i].d_nocase ), NameTag("nc") );
		w.writeSlot( DataCell().setBool( m.d_items[i].d_invert ), NameTag("inv") );
		w.writeSlot( DataCell().setUInt8( m.d_items[i].d_coll ), NameTag("coll") );
		w.endFrame();
	}
	return w.getStream();
}

static void readIndexMeta( const QByteArray& data, IndexMeta& m )
{
	m = IndexMeta();
	IndexMeta::Item item;
	DataReader reader( data );
	bool inItem = false;
	for( DataReader::Token t = reader.nextToken(); DataReader::isUseful( t ); 
		t = reader.nextToken() )
	{
		switch( t )
		{
		case DataReader::Slot:
			{
				const NameTag name = reader.getName().getTag();
				DataCell value;
				reader.readValue( value );
				if( name == "atom" )
					item.d_atom = value.getAtom();
				else if( name == "nc" )
					item.d_nocase = value.getBool();
				else if( name == "inv" )
					item.d_invert = value.getBool();
				else if( name == "coll" )
					item.d_coll = value.getUInt8();
				else if( name == "kind" )
					m.d_kind = (IndexMeta::Kind)value.getUInt8();
			}
			break;
		case DataReader::BeginFrame:
			if( inItem )
				goto error;
			if( reader.getName().getTag() == "item" )
				inItem = true;
			break;
		case DataReader::EndFrame:
			if( !inItem )
				goto error;
			inItem = false;
			m.d_items.append( item );
			item = IndexMeta::Item();
			break;
		default:
			goto error;
		}
	}
	return;
error:
	throw DatabaseException( DatabaseException::DatabaseMeta, 
		"invalid index meta format" );
}

quint32 Database::createIndex( const QByteArray& name, const IndexMeta& meta )
{
	checkOpen();
	Lock lock( this, true );
	if( findIndex( name ) != 0 )
		throw DatabaseException( DatabaseException::IndexExists );

	assert( !meta.d_items.isEmpty() );
	const quint32 table = d_db->createTable();
	BtreeCursor cur;
	cur.open( d_db, getIdxTable(), true );
	const QByteArray id = DataCell().setId32( table ).writeCell();
	cur.insert( DataCell().setLatin1( name ).writeCell(), id );
	cur.insert( id, writeIndexMeta( meta ) );
	// Nur das erste Item wird hier mittels Atom indiziert, da alle weiteren Items im 
	// gleichen Moment mit dem ersten behandelt werden. Null-Werte werden nicht indiziert,
	// bzw. wenn der Wert eines Elements Null ist, wird der Eintrag nicht gemacht.
	cur.insert( DataCell().setAtom( meta.d_items[0].d_atom ).writeCell() + id, id );
	d_idxMeta[table] = meta;
	return table;
}

bool Database::getIndexMeta( quint32 id, IndexMeta& m )
{
	checkOpen();
	Lock lock( this, false );
	QHash<quint32,IndexMeta>::const_iterator i = d_idxMeta.find( id );
	if( i != d_idxMeta.end() )
	{
		m = i.value();
		return true;
	}
	BtreeCursor cur;
	cur.open( d_db, getIdxTable(), false );
	if( cur.moveTo( DataCell().setId32( id ).writeCell() ) )
	{
		readIndexMeta( cur.readValue(), m );
		d_idxMeta[id] = m;
		return true;
	}else
		return false;
}

QList<quint32> Database::findIndexForAtom( quint32 atom )
{
	checkOpen();
	Lock lock( this, false );
	QHash<quint32,QList<quint32> >::const_iterator i = d_idxAtoms.find( atom );
	if( i != d_idxAtoms.end() )
		return i.value();
	QList<quint32> idx;
	BtreeCursor cur;
	cur.open( d_db, getIdxTable(), true );
	const QByteArray key = DataCell().setAtom( atom ).writeCell();
	if( cur.moveTo( key, true ) ) do
	{
		DataCell id;
		id.readCell( cur.readValue() );
		if( id.getId32() )
			idx.append( id.getId32() );
	}while( cur.moveNext( key ) );
	d_idxAtoms[atom] = idx;
	return idx;
}

OID Database::derefUuid( const QUuid& uuid )
{
	BtreeCursor cur;
	cur.open( d_db, getObjTable(), false );
	if( cur.moveTo( DataCell().setUuid( uuid ).writeCell() ) )
	{
		DataCell id;
		id.readCell( cur.readValue() );
		return id.getId64();
	}else
		return 0;
}

void Database::setUuid( OID orl, const QUuid& uuid )
{
	BtreeStore::Txn txn( d_db );
	BtreeCursor cur;
	cur.open( d_db, getObjTable(), true );
	if( orl == 0 )
	{
		if( cur.moveTo( DataCell().setUuid( uuid ).writeCell() ) )
			cur.remove();
	}else
		cur.insert( DataCell().setUuid( uuid ).writeCell(), 
			DataCell().setId64( orl ).writeCell() );
}

RecordImp* Database::createRecord( Record::Type type )
{
	const OID id = getNextOid();

	RecordImp* ri = new RecordImp( this, id, type );
	d_cache[id] = ri;
	return ri;
}

QString Database::getFilePath() const
{
	return QString::fromUtf8( d_db->getPath() );
}
