#ifndef __Sdb_DbStream__
#define __Sdb_DbStream__

#include <QIODevice>
#include <Stream/DataCell.h>
#include <Sdb/Globals.h>

namespace Sdb
{
	class Transaction;

	class DbStream : public QIODevice
	{
		Q_OBJECT
	public:
		DbStream( Transaction*, quint32 sid, QIODevice* imp, bool write = false );
		~DbStream();

		quint32 getSid() const { return d_sid; }
		void erase(); // funktioniert nur für Write-Streams
		StreamMeta getMeta() const;

		void copyFrom( QIODevice* in );
		void copyTo( QIODevice* out );

		Transaction* getTxn() const;

		// Overrides
		bool open ( OpenMode mode ) { return false; }
		bool atEnd () const ;
		qint64 bytesAvailable () const ;
		qint64 bytesToWrite () const;
		bool canReadLine () const;
		void close ();
		bool isSequential() const;		
		qint64 pos () const;
		bool reset ();
		bool seek ( qint64 pos );
		qint64 size () const ;
		bool waitForBytesWritten ( int msecs ) ;
		bool waitForReadyRead ( int msecs );
	protected:

		// Overrides
		qint64 readData ( char * data, qint64 maxSize );
		qint64 writeData ( const char * data, qint64 maxSize );
	protected slots:	
		void onBytesWritten ( qint64 bytes );
		void onReadyRead ();
	private:
		QIODevice* d_imp;
		quint32 d_sid;
	};
}

#endif