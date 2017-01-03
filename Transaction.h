#ifndef __Sdb_Transaction__
#define __Sdb_Transaction__

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

#include <QObject>
#include <QHash>
#include <Sdb/Obj.h>
#include <Sdb/Rel.h>
#include <Sdb/Idx.h>
#include <Sdb/Database.h>
#include <Sdb/DbStream.h>
#include <QUuid>
#include <Sdb/Record.h>
#include <Sdb/Globals.h>
#include <Sdb/Exceptions.h>

namespace Sdb
{
	class RecordCow;

	// Wird von genau einem Thread verwendet und ev. von diesem erzeugt. Enthält alle
	// COW-Records. Wird sowohl für Read als auch Write verwendet und
	// entscheidet selber, wann COW angelegt werden muss.
	class Transaction : public QObject
	{
	public:
		class Auto
		{
		public:
			Auto( Transaction* );
			~Auto();
			void rollback();
			void commit();
		private:
			Transaction* d_txn;
		};
		Transaction( Database*, QObject* owner = 0 );
		~Transaction();

		// begin() gibt es nicht. Txn wird automatisch gestartet bei lock, create, set, erase
		void commit();
		void rollback();
		bool isActive() const { return d_inTxn; }

		Orl getOrl( OID oid ) const;

		Obj createObject( Atom type = 0 );
        Obj createObject( const QUuid&, Atom type = 0 );
		Obj getObject( OID oid ) const;
		Obj getObject( const Stream::DataCell& ) const;
		Obj getObject( const QUuid& ) const;
        Obj getOrCreateObject( const QUuid&, Atom type = 0 );

		Lit getElement( OID bookmark ) const;

		Rel getRelation( OID rid ) const;
		Rel getRelation( const Stream::DataCell& ) const;
		Rel getRelation( const QUuid& ) const;

		// Nicht in Transaktion
		DbStream* createStream( StreamMeta = StreamMeta() );
		DbStream* getStream( quint32 sid, bool writing = false ) const; // 0..locked
		// -

		Database* getDb() const { return d_db; }
		// Convenience
		Atom getAtom( const QByteArray& name );
		Idx findIndex( const QByteArray& name ) const;
	protected: // nur für folgende friends zugänglich
		friend class Obj;
		friend class Orl;
		friend class Lit;
		friend class Qit;
		friend class Rel;
		friend class DbStream;
		// Implementation von Obj, Orl etc.
		quint32 createQSlot( Record*, const Stream::DataCell& );
		void getQSlot( Record*, quint32 nr, Stream::DataCell& ) const;
		void getCell( Record*, const QList<Stream::DataCell>& key, Stream::DataCell& value ) const;
		void setCell( Record*, const QList<Stream::DataCell>& key, const Stream::DataCell& value );
		void getField( Record*, quint32 id, Stream::DataCell& ) const;
		bool hasField( Record*, quint32 id ) const;
		quint64 getIdField( Record*, quint32 id ) const; // Helper für getField
		void erase( Record* );
		void setQSlot( Record*, quint32 nr, const Stream::DataCell& );
		void setField( Record*, quint32 id, const Stream::DataCell& );
		Record* getRecord( quint64 oid, Record::Type = Record::TypeUndefined ) const; // Undefined..don't care
		RecordCow* createRecord( Record::Type type );
		void cleanCache();
		RecordCow* lockImp( Record* ); 
		bool loadStreamMeta( quint32, StreamMeta& ); // false..not found
		void unlockStream( quint32 );
		void removeFromIndex(quint64 id, const Record::Fields& all, quint32 atom,const Stream::DataCell&);
		void removeFromIndex( quint64 id,const Record::Fields& all, const Record::Fields& focus );
		void addToIndex( quint64 id, const Record::Fields& all, quint32 atom,const Stream::DataCell& );
		void addToIndex( quint64 id, const Record::Fields& all, const Record::Fields& focus );
		OID derefUuid( const QUuid& );
		void dump( Record* );
	private: // ganz privat
		QHash<OID,RecordCow*> d_cache;
		Database* d_db; // Database ist nicht parent, da ev. Txn in anderem Thread erzeugt.
		QList<UpdateInfo> d_notify;
		bool d_inTxn;
	};
}

#endif
