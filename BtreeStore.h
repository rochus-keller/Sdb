#ifndef __Sdb_BtreeStore__
#define __Sdb_BtreeStore__

#include <QObject>

struct Btree;
struct sqlite3;

namespace Sdb
{
	class System;

	class BtreeStore : public QObject
	{
	public:
		class Txn
		{
		public:
			Txn( BtreeStore* );
			~Txn();
			void rollback();
			void commit();
		private:
			BtreeStore* d_db;
		};
		BtreeStore( QObject* owner = 0 );
		~BtreeStore();

		void open( const QByteArray& path );
		void close();
		Btree* getBt() const;

		int createTable(bool noData = false);
		void dropTable( int table );
		void clearTable( int table );

		// Meta-Access
		void writeMeta( const QByteArray& key, const QByteArray& val );
		QByteArray readMeta( const QByteArray& key ) const;
		void eraseMeta( const QByteArray& key );
		int getMetaTable() const { return d_metaTable; }

		// Write-Transactions
		void transBegin(); 
		void transCommit();
		void transAbort();
		bool isTrans() const { return d_txnLevel > 0; }

		const QByteArray& getPath() const { return d_path; }
	protected:
		void checkOpen() const;
	private:
		friend class ReadLock;
		friend class Txn;
		sqlite3* d_db;
		qint32 d_txnLevel;
		int d_metaTable;
		QByteArray d_path; // UTF-8
	};
}


#endif