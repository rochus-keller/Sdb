#ifndef __Sdb_Database__
#define __Sdb_Database__

#include <QObject>
#include <QMutex>
#include <QHash>
#include <Sdb/Globals.h>
#include <Sdb/Record.h>
#include <Sdb/UpdateInfo.h>

namespace Sdb
{
	class BtreeStore;
	class RecordImp;

	// Hauptklasse für den Client-Zugriff.
	// Versteckt Btree. Wird von mehreren Threads parallel gebraucht.

	class Database : public QObject
	{ 
		Q_OBJECT
	public:
		class Lock
		{
		public:
			Lock( Database*, bool txn = false );
			~Lock();
			void rollback();
			void commit();
		private:
			Database* d_db;
			bool d_txn; // Starte zudem DB-Transaktion
		};
		Database( QObject* = 0 );
		~Database();

		void open( const QString& path );
		void close();

		void presetAtom( const QByteArray& name, Atom atom );
		Atom getAtom( const QByteArray& name, bool create = true );
		QByteArray getAtomString( Atom );

		Index createIndex( const QByteArray& name, const IndexMeta& );
		Index findIndex( const QByteArray& name );
		bool getIndexMeta( Index, IndexMeta& );
		QList<Index> findIndexForAtom( Atom atom );

		void checkUsed( OID );
		OID getMaxOid() { return getNextOid( false ); }

		void addObserver( QObject*, const char* slot );
		void removeObserver( QObject*, const char* slot );

		void dump();
        void dumpAtoms();
		void dumpQueue( OID = 0 );
		QString getFilePath() const;
	signals:
		void notify( Sdb::UpdateInfo );
		void doCheckUsed( quint64 ); // OID
	protected slots:
		void onCheckUsed( quint64 ); // OID
	private: // nur für Transaction zugänglich
		friend class Transaction;
		RecordImp* getOrLoadRecord( quint64 );
		void saveRecord( RecordImp* );
		void eraseRecord( RecordImp* );
		int getObjTable();
		int getStrTable();
		int getDirTable();
		int getIdxTable();
		int getQueTable();
		int getMapTable();
		quint32 getNextQueueNr(quint64 oid);
		quint32 getNextSid();
		OID getNextOid(bool persist = true);
		void checkOpen() const;
		void loadMeta();
		void saveMeta();
		QString getStreamsDir() const;
		bool lockStream( quint32, bool write = false );
		bool unlockStream( quint32 );
		bool isStreamWriteLocked( quint32 ) const;
		void saveStreamMeta( quint32 id, const StreamMeta& );
		bool loadStreamMeta( quint32, StreamMeta& ); // false..not found
		quint64 derefUuid( const QUuid& );
		void setUuid( quint64 orl, const QUuid& ); // orl==0..remove
		RecordImp* createRecord( Record::Type type );

	private: // nur für Idx zugänglich
		friend class Idx;
		friend class Qit;
		friend class Mit;
		BtreeStore* getStore() const { return d_db; }

	private: // nur für Database und Lock zugänglich
		friend class Lock;

		struct Meta
		{
			Meta():d_objTable(0),d_dirTable(0),d_strTable(0),d_idxTable(0),d_queTable(0),d_mapTable(0) {}

			int d_objTable; // Btree mit ID->Record und UUID->ID
			int d_dirTable; // Btree mit Atom->Name und Name->Atom
			int d_strTable; // Btree mit ID->Stream
			int d_idxTable; // Btree mit ID->Indexdef und Atom, ID um Indizes aufzufinden
			int d_queTable; // Btree mit <oid> <nr> -> <cell>
			int d_mapTable; // Btree mit <oid> [ <cell> ]* -> <cell>
		};
		Meta d_meta;

		// Wir verwenden Mutex und nicht ReadWriteLock, da alle Threads dasselbe BtreeStore
		// verwenden und sonst in die Transaktion eines anderen Thread sehen würden.
		//QMutex d_lock;
		BtreeStore* d_db;
		QHash<OID,RecordImp*> d_cache;
		QHash<QByteArray,Atom> d_dir;
		QHash<Atom,QByteArray> d_invDir;
		QHash<quint32,int> d_streamLocks; // negativ..writelock, positiv..readlocks
		QHash<Index,IndexMeta> d_idxMeta; // Cache, bleibt im Speicher
		QHash<Atom,QList<Index> > d_idxAtoms; // Cache von Atom -> Idx
	};
}

#endif
