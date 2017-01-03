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

#include "Transaction.h"
#include "Database.h"
#include "Exceptions.h"
#include "RecordCow.h"
#include "RecordImp.h"
#include "DbStream.h"
#include "BtreeCursor.h"
#include "Idx.h"
#include <QList>
#include <QFile>
#include <QDir>
#include <cassert>
using namespace Sdb;
using namespace Stream;

Transaction::Auto::Auto( Transaction* txn ):d_txn(txn)
{
	assert( txn ); 
}

void Transaction::Auto::rollback()
{
	d_txn->rollback();
}

void Transaction::Auto::commit()
{
	d_txn->commit();
}

Transaction::Auto::~Auto()
{
	rollback();
}

Transaction::Transaction( Database* db, QObject* owner ):
	QObject( owner ), d_db(db),d_inTxn(false)
{
	assert( d_db != 0 );
}

Transaction::~Transaction()
{
	if( d_inTxn )
		rollback();
	QHashIterator<OID,RecordCow*> i( d_cache );
	while( i.hasNext() ) 
	{
		i.next();
		delete i.value();
	}
}

Obj Transaction::getObject( const Stream::DataCell& v ) const
{
	if( v.isOid() )
		return getObject( v.getOid() );
	else if( v.isUuid() )
		return getObject( v.getUuid() );
	else
		return Obj();
}

Obj Transaction::getObject( const QUuid& uuid ) const
{
	Database::Lock lock( d_db, false );
	const OID id = d_db->derefUuid( uuid );
	Record* r = getRecord( id, Record::TypeObject );
	if( r == 0 )
		return Obj(); 
	else
        return Obj( r, const_cast<Transaction*>(this) );
}

Obj Transaction::createObject( const QUuid& uuid, Atom type )
{
	// TODO: sicherstellen, dass nicht bereits ein Objekt mit der gegebenen uuid existiert
	Obj o = createObject( type );
    o.setGuid( uuid );
	return o;
}

Obj Transaction::getOrCreateObject(const QUuid & uuid, Atom type)
{
    Obj o = getObject( uuid );
	if( o.isNull() )
		o = createObject( uuid, type );
	return o;
}

Rel Transaction::getRelation( const QUuid& uuid ) const
{
	Database::Lock lock( d_db, false );
	const OID id = d_db->derefUuid( uuid );
	Record* r = getRecord( id, Record::TypeRelation );
	if( r == 0 )
		return Rel(); 
	else
		return Rel( r, const_cast<Transaction*>(this) );
}

Record* Transaction::getRecord( OID id, Record::Type type ) const
{
	if( id == 0 )
		return 0;
	Record* r = d_cache.value( id );
	if( r == 0 )
	{
		Database::Lock lock( d_db, false );
		r = d_db->getOrLoadRecord( id );
	}
	if( r == 0 || r->isDeleted() )
		return 0;
	if( type != Record::TypeUndefined && r->getType() != type )
		throw DatabaseException( DatabaseException::WrongType );
	return r;
}

Orl Transaction::getOrl( OID oid ) const
{
	Record* r = getRecord( oid, Record::TypeUndefined );
	if( r == 0 )
		return Orl();
	else
		return Orl( r, const_cast<Transaction*>( this ) );
}

Obj Transaction::getObject( OID oid ) const
{
	Record* r = getRecord( oid, Record::TypeObject );
	if( r == 0 )
		return Obj();
	else
		return Obj( r, const_cast<Transaction*>( this ) );
}

RecordCow* Transaction::createRecord( Record::Type type )
{
	d_inTxn = true;

	Database::Lock lock( d_db, false );
	RecordImp* ri = d_db->createRecord( type );
	ri->d_state = RecordImp::StateNew;

	RecordCow* rc = new RecordCow( ri, this );
	ri->d_cow = rc;
	d_cache[ri->getId()] = rc;

	return rc;
}

Obj Transaction::createObject( quint32 type )
{
	RecordCow* rc = createRecord( Record::TypeObject );

	if( type && type >= Record::MinReservedField )
		throw DatabaseException(DatabaseException::ReservedName );
	if( type )
		rc->d_fields[Record::FieldType].setAtom( type );

	UpdateInfo c;
	c.d_kind = UpdateInfo::ObjectCreated;
	c.d_id = rc->getId();
	c.d_name = type;
	d_notify.append( c );

	return Obj( rc, this );
}

Rel Transaction::getRelation( OID rid ) const
{
	Record* r = getRecord( rid, Record::TypeRelation );
	if( r == 0 )
		return Rel();
	else
		return Rel( r, const_cast<Transaction*>( this ) );
}

Rel Transaction::getRelation( const Stream::DataCell& v ) const
{
	if( v.isRid() )
		return getRelation( v.getRid() );
	else if( v.isUuid() )
		return getRelation( v.getUuid() );
	else
		return Rel();
}

static void _saveQueue( RecordCow* r, BtreeStore* db, int table )
{
	BtreeCursor cur;
	cur.open( db, table, true );
	const QByteArray oid = DataCell().setId64( r->getId() ).writeCell();

	QMap<quint32,Stream::DataCell>::const_iterator j;
	for( j = r->getQueue().begin(); j != r->getQueue().end(); ++j )
	{
		const QByteArray nr = DataCell().setId32( j.key() ).writeCell();
		if( j.value().isNull() )
		{
			if( cur.moveTo( oid + nr ) )
				cur.remove();
		}else
		{
			cur.insert( oid + nr, j.value().writeCell( false, true ) ); // RISK: compression
		}
	}
}

static void _saveMap( RecordCow* r, BtreeStore* db, int table )
{
	BtreeCursor cur;
	cur.open( db, table, true );
	const QByteArray oid = DataCell().setOid( r->getId() ).writeCell();

	QMap<QByteArray,Stream::DataCell>::const_iterator j;
	for( j = r->getMap().begin(); j != r->getMap().end(); ++j )
	{
		if( j.value().isNull() )
		{
			if( cur.moveTo( oid + j.key() ) )
				cur.remove();
		}else
		{
			cur.insert( oid + j.key(), j.value().writeCell( false, true ) ); // RISK: compression
		}
	}
}

static void _eraseQueue( RecordCow* r, BtreeStore* db, int table )
{
	BtreeCursor cur;
	cur.open( db, table, true );
	const QByteArray oid = DataCell().setId64( r->getId() ).writeCell();
	if( cur.moveTo( oid, true ) ) do
	{
		cur.remove();
	}while( cur.moveNext() && cur.readKey().startsWith( oid ) );
}

static void _eraseMap( RecordCow* r, BtreeStore* db, int table )
{
	BtreeCursor cur;
	cur.open( db, table, true );
	const QByteArray oid = DataCell().setOid( r->getId() ).writeCell();
	if( cur.moveTo( oid, true ) ) do
	{
		cur.remove();
	}while( cur.moveNext() && cur.readKey().startsWith( oid ) );
}

void Transaction::commit()
{
	if( !d_inTxn )
		return;
	d_inTxn = false;
	Database::Lock lock( d_db, true );
	QHash<OID,RecordCow*>::const_iterator i;
	for( i = d_cache.begin(); i != d_cache.end(); ++i )
	{
		if( i.value()->d_imp->d_cow == i.value() )
		{
			// Der COW zeigt auf einen IMP
			i.value()->d_imp->d_cow = 0; // unlock
			if( i.value()->d_imp->d_state == RecordImp::StateToDelete )
			{
				// Der Record ist zum löschen vorgemerkt. Vollziehe die Löschung
				removeFromIndex( i.value()->d_imp->d_id, i.value()->d_imp->d_fields,
					i.value()->d_imp->d_fields );
				d_db->eraseRecord( i.value()->d_imp );
				_eraseQueue( i.value(), d_db->getStore(), d_db->getQueTable() );
				_eraseMap( i.value(), d_db->getStore(), d_db->getMapTable() );
				i.value()->d_imp->d_state = RecordImp::StateDeleted;
			}else if( i.value()->d_imp->d_state == RecordImp::StateNew )
			{
				// Der Record ist ganz neu.
				// IMP darf daher noch keine Werte enthalten
				assert( i.value()->d_imp->d_fields.isEmpty() );
				i.value()->d_imp->d_fields = i.value()->d_fields;
				addToIndex( i.value()->d_imp->d_id, i.value()->d_imp->d_fields, 
					i.value()->d_imp->d_fields );
				d_db->saveRecord( i.value()->d_imp );
				_saveQueue( i.value(), d_db->getStore(), d_db->getQueTable() );
				if( !i.value()->getMap().isEmpty() )
					_saveMap( i.value(), d_db->getStore(), d_db->getMapTable() );
				i.value()->d_imp->d_state = RecordImp::StateIdle;
			}else if( !i.value()->d_fields.isEmpty() )
			{
				// Es gab Änderungen. Übertrage diese in IMP und speichere Record
				Record::Fields::const_iterator j;
				for( j = i.value()->d_fields.begin(); j != i.value()->d_fields.end(); ++j )
				{
					Record::Fields::iterator k = i.value()->d_imp->d_fields.find( j.key() );
					if( k != i.value()->d_imp->d_fields.end() )
					{
						if( !k.value().isNull() )
							removeFromIndex( i.value()->d_imp->d_id, i.value()->d_imp->d_fields,
								j.key(), k.value() );
						k.value() = j.value();
					}else
						i.value()->d_imp->d_fields.insert( j.key(), j.value() );
				}
				// TODO: es müssen hier auch Änderungen an Feldern in kombinierten Idizes
				// berücksichtigt werden, die nicht die ersten im Index sind!
				addToIndex( i.value()->d_imp->d_id, i.value()->d_imp->d_fields, 
					i.value()->d_fields );
				d_db->saveRecord( i.value()->d_imp );
				_saveQueue( i.value(), d_db->getStore(), d_db->getQueTable() );
				if( !i.value()->getMap().isEmpty() )
					_saveMap( i.value(), d_db->getStore(), d_db->getMapTable() );
			}else
			{
				_saveQueue( i.value(), d_db->getStore(), d_db->getQueTable() );
				if( !i.value()->getMap().isEmpty() )
					_saveMap( i.value(), d_db->getStore(), d_db->getMapTable() );
			}
			i.value()->d_fields.clear();
			i.value()->d_queue.clear();
			i.value()->d_map.clear();
		}else
			// Nur COW, welche auf durch sie gelockten IMP zeigen dürfen Daten enthalten
			assert( i.value()->d_fields.isEmpty() );
	}
	for( int i = 0; i < d_notify.size(); i++ )
	{
		try
		{
			emit d_db->notify( d_notify[i] );
		}catch( ... )
		{
			// RISK: ev. Exceptions abfangen
		}
	}
	d_notify.clear();
	cleanCache();
}

void Transaction::removeFromIndex( OID id, const Record::Fields& all, const Record::Fields& f )
{
	Record::Fields::const_iterator j;
	for( j = f.begin(); j != f.end(); ++j )
		removeFromIndex( id, all, j.key(), j.value() );
}

void Transaction::removeFromIndex(OID id, const Record::Fields& all, quint32 a,const Stream::DataCell& f)
{
	if( a == Record::FieldUuid && f.getType() == DataCell::TypeUuid )
	{
		// Spezialregelung für Uuids.
		d_db->setUuid( 0, f.getUuid() );
	}
	const QByteArray idstr = DataCell().setId64( id ).writeCell();
	QByteArray key;
	QList<quint32> idx = d_db->findIndexForAtom( a );
	// Gehe durch alle Indizes, in denen das Atom das erste Element ist
	for( int i = 0; i < idx.size(); i++ )
	{
		key.clear();
		key.reserve( 255 ); // RISK
		IndexMeta meta;
		if( d_db->getIndexMeta( idx[i], meta ) )
		{
			if( meta.d_kind == IndexMeta::Fulltext )
			{
				assert( meta.d_items.size() == 1 );
				assert( meta.d_items[0].d_atom == a );
				if( !f.isNull() )
				{
					// TODO: Text in Worte zerlegen, Text aus Html, Xml, Bml extrahieren
					
				}
			}else if( meta.d_kind == IndexMeta::Value ||
				meta.d_kind == IndexMeta::Unique )
			{
				assert( !meta.d_items.isEmpty() );
				assert( meta.d_items[0].d_atom == a );
				bool hasNulls = false;
				if( f.isNull() )
					hasNulls = true;
				else
					Idx::addElement( key, meta.d_items[0], f );
				for( int j = 1; j < meta.d_items.size() && !hasNulls; j++ )
				{
					Record::Fields::const_iterator it = all.find( meta.d_items[j].d_atom );
					if( it == all.end() || it.value().isNull() )
					{
						hasNulls = true;
					}else
						Idx::addElement( key, meta.d_items[j], it.value() );
				}
				if( !hasNulls )
				{
					if( meta.d_kind == IndexMeta::Value )
						key += idstr;
					
					BtreeCursor cur;
					cur.open( d_db->getStore(), idx[i], true );
					if( cur.moveTo( key ) )
					{
						// Bei Unique Index nur die Indizes für die eigene ID entfernen.
						if( meta.d_kind != IndexMeta::Unique || cur.readValue() != idstr )
							cur.remove();
					}
				}
			}
		}
	}
}

void Transaction::addToIndex(OID id, const Record::Fields& all, quint32 a,const Stream::DataCell& f)
{
	if( a == Record::FieldUuid && f.getType() == DataCell::TypeUuid )
	{
		// Spezialregelung für Uuids.
		d_db->setUuid( id, f.getUuid() );
	}
	const QByteArray idstr = DataCell().setId64( id ).writeCell();
	QByteArray key;
	QList<quint32> idx = d_db->findIndexForAtom( a );
	// Gehe durch alle Indizes, in denen das Atom das erste Element ist
	for( int i = 0; i < idx.size(); i++ )
	{
		key.clear();
		key.reserve( 255 ); // RISK
		IndexMeta meta;
		if( d_db->getIndexMeta( idx[i], meta ) )
		{
			if( meta.d_kind == IndexMeta::Fulltext )
			{
				assert( meta.d_items.size() == 1 );
				assert( meta.d_items[0].d_atom == a );
				if( !f.isNull() )
				{
					// TODO: Text in Worte zerlegen, Text aus Html, Xml, Bml extrahieren
					
				}
			}else if( meta.d_kind == IndexMeta::Value ||
				meta.d_kind == IndexMeta::Unique )
			{
				assert( !meta.d_items.isEmpty() );
				assert( meta.d_items[0].d_atom == a );
				bool hasNulls = false;
				if( f.isNull() )
					hasNulls = true;
				else
					Idx::addElement( key, meta.d_items[0], f );
				for( int j = 1; j < meta.d_items.size() && !hasNulls; j++ )
				{
					Record::Fields::const_iterator it = all.find( meta.d_items[j].d_atom );
					if( it == all.end() || it.value().isNull() )
					{
						hasNulls = true;
					}else
						Idx::addElement( key, meta.d_items[j], it.value() );
				}
				if( !hasNulls )
				{
					if( meta.d_kind == IndexMeta::Value )
						key += idstr;
					
					BtreeCursor cur;
					cur.open( d_db->getStore(), idx[i], true );
					cur.insert( key, idstr );
				}
			}
		}
	}
}

void Transaction::addToIndex( OID id, const Record::Fields& all, const Record::Fields& f )
{
	// NOTE: in f sind nur die geänderten Felder. Da wir für kombinierte Felder ev.
	// auch Zugriff auf bestehende, nicht geänderte Felder benötigen, muss auch all
	// mitgeliefert werden!
	Record::Fields::const_iterator j;
	for( j = f.begin(); j != f.end(); ++j )
		addToIndex( id, all, j.key(), j.value() );
}

void Transaction::rollback()
{
	if( !d_inTxn )
		return;
	d_inTxn = false;
	{
		Database::Lock lock( d_db, false );
		QHash<OID,RecordCow*>::const_iterator i;
		for( i = d_cache.begin(); i != d_cache.end(); ++i )
		{
			i.value()->d_fields.clear();
			i.value()->d_queue.clear();
			i.value()->d_map.clear();
			assert( i.value()->d_imp != 0 );
			if( i.value()->d_imp->d_cow == i.value() )
			{
				// Der COW zeigt auf einen IMP
				i.value()->d_imp->d_cow = 0; // unlock
				if( i.value()->d_imp->d_state == RecordImp::StateNew )
				{
					// Es ist bei neuen Objekten vor Commit nicht möglich, dass schon
					// Werte in d_imp sind.
					assert( i.value()->d_imp->d_fields.isEmpty() );
					// Markiere neue Objekte als gelöscht, damit sie nicht weiterverwendet
					// werden können. Wir löschen die Records nicht, da noch referenzen
					// darauf existieren könnten.
					i.value()->d_imp->d_state = RecordImp::StateDeleted;
				}else if( i.value()->d_imp->d_state == RecordImp::StateToDelete )
				{
					// IMP konnte nur von dieser Transaktion als gelöscht markiert werden.
					// also löschen rückgängig machen.
					i.value()->d_imp->d_state = RecordImp::StateIdle;
				}
			}
		}
	}
	d_notify.clear();
	cleanCache();
}

void Transaction::cleanCache()
{
	// In einer Transaktion könnte selbst ein Rec mit refCount=0 ungesicherte Daten enthalten
	assert( !d_inTxn ); 

	QList<OID> remove;
	QMutableHashIterator<OID,RecordCow*> i( d_cache );
	while( i.hasNext() ) 
	{
		i.next();
		if( i.value()->d_refCount <= 0 )
		{
			// Da wir nicht in Txn sind, darf das von imp referenzierte Objekt nicht von
			// d_imp gelockt sein. Es könnte aber von einer anderen Transaktion gelockt sein.
			assert( i.value()->d_imp != 0 );
			assert( i.value()->d_imp->d_cow != i.value() );
			delete i.value();
			/* funktioniert nicht
			i.next();
			i.remove();
			i.previous();
			*/
			remove.append( i.key() );
		}
	} 
	QList<OID>::const_iterator j;
	for( j = remove.begin(); j != remove.end(); ++j )
		d_cache.remove( *j );
}

RecordCow* Transaction::lockImp( Record* r )
{
	d_inTxn = true;
	RecordCow* rc = dynamic_cast<RecordCow*>( r );
	if( rc == 0 )
	{
		// Es wurde kein COW gegeben, also muss es ein IMP sein.
		Database::Lock lock( d_db, false );
		RecordImp* ri = dynamic_cast<RecordImp*>( r );
		assert( ri != 0 );
		if( ri->d_cow )
		{
			// ri ist bereits locked. Prüfen ob von dieser Transaktion.
			if( ri->d_cow->d_txn != this )
				throw DatabaseException( DatabaseException::RecordLocked );
			else
				// Wir nehmen den existierenden COW
				rc = ri->d_cow;
		}else
		{
			// ri ist noch nicht gelockt. 
			// Es könnte dennoch sein, dass bereits ein COW zur ID existiert.
			// Dies ist möglich nach einem commit oder rollback
			rc = d_cache.value( ri->d_id );
			if( rc == 0 )
			{
				rc = new RecordCow( ri, this );
				d_cache[ri->d_id] = rc;
			}
			// Locke den IMP.
			ri->d_cow = rc;
		}
	}else
	{
		// Es wurde ein COW übergeben. 
		Database::Lock lock( d_db, false );
		assert( rc->d_imp );
		if( rc->d_imp->d_cow )
		{
			// ri ist bereits locked. Prüfen ob von dieser Transaktion.
			if( rc->d_imp->d_cow->d_txn != this )
				throw DatabaseException( DatabaseException::RecordLocked );
		}else
		{
			// ri ist noch nicht gelockt. Locke den IMP.
			rc->d_imp->d_cow = rc;
		}
	}
	return rc;
}

void Transaction::erase( Record* r )
{
	d_inTxn = true;
	RecordCow* rc = lockImp( r );
	Database::Lock lock( d_db, false );
	assert( rc->d_imp );
	assert( rc->d_imp->d_cow = rc );
	// Man kann nicht mehrmals löschen
	if( rc->d_imp->d_state == RecordImp::StateDeleted )
		throw DatabaseException( DatabaseException::RecordDeleted );
	// Markiere IMP als gelöscht zur Ausführung in commit
	rc->d_imp->d_state = RecordImp::StateToDelete;
}

void Transaction::getField( Record* r,quint32 id, Stream::DataCell& v ) const
{
	// Wir müssen den Wert v kopieren, da eine Referenz nicht Threadsicher wäre

	RecordImp* ri = dynamic_cast<RecordImp*>( r );
	Database::Lock lock( d_db, false );
	if( ri )
	{
		if( ri->d_cow && ri->d_cow->d_txn == this )
			// Es existiert ein COW das ev. Daten enthält
			v = ri->d_cow->getField( id );
		else
			// Es existiert kein COW
			v = ri->getField( id );
	}else
	{
		RecordCow* rc = dynamic_cast<RecordCow*>( r );
		assert( rc );
		v = rc->getField( id );
		// TODO: ev. unterscheiden betr. Lock, ob Objekt lokal in COW oder in IMP
	}
}

bool Transaction::hasField( Record* r,quint32 id ) const
{
	// im Wesentlichen Kopie von Transaction::getField
	RecordImp* ri = dynamic_cast<RecordImp*>( r );
	Database::Lock lock( d_db, false );
	if( ri )
	{
		if( ri->d_cow && ri->d_cow->d_txn == this )
			// Es existiert ein COW das ev. Daten enthält
			return !ri->d_cow->getField( id ).isNull();
		else
			// Es existiert kein COW
			return !ri->getField( id ).isNull();
	}else
	{
		RecordCow* rc = dynamic_cast<RecordCow*>( r );
		assert( rc );
		return rc->getField( id ).isNull();
	}
	return false;
}

void Transaction::getQSlot( Record* r,quint32 nr, Stream::DataCell& v ) const
{
	v.setNull();
	if( r == 0 )
		return;
	Database::Lock lock( d_db, false );
	RecordCow* rc = dynamic_cast<RecordCow*>( r );
	if( rc == 0 )
	{
		RecordImp* ri = dynamic_cast<RecordImp*>( r );
		rc = ri->d_cow;
	}
	if( rc != 0 && nr != 0 )
	{
		QMap<quint32,Stream::DataCell>::const_iterator i = rc->d_queue.find( nr );
		if( i != rc->d_queue.end() )
		{
			v = i.value();
			return;
		}
	}
	BtreeCursor cur;
	cur.open( d_db->getStore(), d_db->getQueTable(), false );
	DataWriter w;
	w.writeSlot( DataCell().setId64( r->getId() ) );
	if( nr != 0 )
		w.writeSlot( DataCell().setId32( nr ) );
	if( cur.moveTo( w.getStream() ) )
	{
		v.readCell( cur.readValue() );
	}
}

void Transaction::getCell( Record* r, const QList<Stream::DataCell>& key, Stream::DataCell& v ) const
{
	v.setNull();
	if( r == 0 )
		return;
	Database::Lock lock( d_db, false );
	RecordCow* rc = dynamic_cast<RecordCow*>( r );
	if( rc == 0 )
	{
		RecordImp* ri = dynamic_cast<RecordImp*>( r );
		rc = ri->d_cow;
	}
	DataWriter _key;
	for( int i = 0; i < key.size(); i++ )
		_key.writeSlot( key[i] );

	if( rc != 0 )
	{
		QMap<QByteArray,Stream::DataCell>::const_iterator i = rc->d_map.find( _key.getStream() );
		if( i != rc->d_map.end() )
		{
			v = i.value();
			return;
		}
	}
	BtreeCursor cur;
	cur.open( d_db->getStore(), d_db->getMapTable(), false );
	DataWriter oid;
	oid.writeSlot( DataCell().setOid( r->getId() ) );
	if( cur.moveTo( oid.getStream() + _key.getStream() ) )
	{
		v.readCell( cur.readValue() );
	}
}

quint32 Transaction::createQSlot( Record* r, const Stream::DataCell& v )
{
	Database::Lock lock( d_db, true );
	const quint32 nr = d_db->getNextQueueNr( r->getId() );
	setQSlot( r, nr, v );
	return nr;
}

OID Transaction::getIdField( Record* r, quint32 id ) const
{
	Stream::DataCell v;
	getField( r, id, v );
	return v.toId64();
}

void Transaction::setQSlot( Record* r, quint32 nr, const Stream::DataCell& v )
{
	d_inTxn = true;
	RecordCow* rc = lockImp( r );
	assert( rc->d_imp );
	assert( rc->d_imp->d_cow = rc );
	if( rc->d_imp->d_state == RecordImp::StateDeleted /*||
		rc->d_imp->d_state == RecordImp::StateToDelete */ )
		throw DatabaseException( DatabaseException::RecordDeleted );
	rc->d_queue[nr] = v;
}

void Transaction::setCell( Record* r, const QList<Stream::DataCell>& key, const Stream::DataCell& v )
{
	d_inTxn = true;
	RecordCow* rc = lockImp( r );
	assert( rc->d_imp );
	assert( rc->d_imp->d_cow = rc );
	if( rc->d_imp->d_state == RecordImp::StateDeleted /* ||
		rc->d_imp->d_state == RecordImp::StateToDelete */ )
		throw DatabaseException( DatabaseException::RecordDeleted );
	DataWriter _key;
	for( int i = 0; i < key.size(); i++ )
		_key.writeSlot( key[i] );
	rc->d_map[_key.getStream()] = v;
}

void Transaction::setField( Record* r, quint32 id, const Stream::DataCell& v )
{
	d_inTxn = true;
	RecordCow* rc = lockImp( r );
	assert( rc->d_imp );
	assert( rc->d_imp->d_cow = rc );
	if( rc->d_imp->d_state == RecordImp::StateDeleted /*|| // Warum sollten wir das verbieten?
		rc->d_imp->d_state == RecordImp::StateToDelete */ )
		throw DatabaseException( DatabaseException::RecordDeleted );
	rc->d_fields[id] = v;
}

quint32 Transaction::getAtom( const QByteArray& name )
{
	return d_db->getAtom( name );
}

Lit Transaction::getElement( OID bookmark ) const
{
	Record* r = getRecord( bookmark, Record::TypeElement );
	if( r == 0 )
		return Lit();
	else
		return Lit( r, const_cast<Transaction*>(this) );
}

DbStream* Transaction::createStream( StreamMeta meta )
{
	// t..zeroTerminated oder explicitLength
	Database::Lock lock( d_db, false );
	const quint32 id = d_db->getNextSid();
	meta.d_lastUse = QDateTime::currentDateTime();
	meta.d_useCount = 1;
	d_db->saveStreamMeta( id, meta );
	const bool res = d_db->lockStream( id, true );
	assert( res );
	QDir dir = d_db->getStreamsDir();
	QFile* f = new QFile( dir.absoluteFilePath( QString::number( id ) ) );
	if( !f->open( QIODevice::WriteOnly ) )
	{
		delete f;
		throw DatabaseException( DatabaseException::StreamFile, "cannot open for writing" );
	}
	return new DbStream( this, id, f, true ); 
}

DbStream* Transaction::getStream( quint32 sid, bool writing ) const
{
	Database::Lock lock( d_db, false );
	StreamMeta meta;
	if( !d_db->loadStreamMeta( sid, meta ) )
		throw DatabaseException( DatabaseException::StreamFile, "invalid sid" );
	if( !d_db->lockStream( sid, writing ) )
		return 0;
	QDir dir = d_db->getStreamsDir();
	QFile* f = new QFile( dir.absoluteFilePath( QString::number( sid ) ) );
	if( !f->open( (writing)?QIODevice::WriteOnly:QIODevice::ReadOnly ) )
	{
		delete f;
		throw DatabaseException( DatabaseException::StreamFile, "cannot open stream file" );
	}
	return new DbStream( const_cast<Transaction*>(this), sid, f, writing ); 
}

bool Transaction::loadStreamMeta( quint32 id, StreamMeta& m )
{
	Database::Lock lock( d_db, false );
	return d_db->loadStreamMeta( id, m );
}

void Transaction::unlockStream( quint32 id )
{
	Database::Lock lock( d_db, false );
	const bool notify = d_db->isStreamWriteLocked( id );
	d_db->unlockStream( id );
	if( notify )
	{
		UpdateInfo c;
		c.d_kind = UpdateInfo::StreamChanged;
		c.d_id = id;
		emit d_db->notify( c ); // da ja Stream nicht Teil der Transaktion ist, direkt mitteilen.
	}
}

OID Transaction::derefUuid( const QUuid& uuid )
{
	return d_db->derefUuid( uuid );
}

Idx Transaction::findIndex( const QByteArray& name ) const
{
	quint32 id = d_db->findIndex( name );
	if( id == 0 )
		return Idx();
	else
		return Idx( const_cast<Transaction*>(this), id );
}

void Transaction::dump( Record* r )
{
	if( r == 0 )
	{
		qDebug( "*** Record is null" );
		return;
	}
	RecordImp* ri = dynamic_cast<RecordImp*>( r );
	if( ri == 0 )
	{
		RecordCow* rc = dynamic_cast<RecordCow*>( r );
		assert( rc );
		ri = rc->d_imp;
	}
	ri->dump();
}
