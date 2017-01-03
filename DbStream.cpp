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

#include "DbStream.h"
#include "Transaction.h"
#include <cassert>
using namespace Sdb;

DbStream::DbStream( Transaction* txn, quint32 sid, QIODevice* imp, bool write ):QIODevice( txn )
{
	d_sid = sid;
	assert( d_sid != 0 );
	assert( txn );
	d_imp = imp;
	d_imp->setParent( this );
	assert( d_imp != 0 );
	QIODevice::open( (write)?QIODevice::WriteOnly:QIODevice::ReadOnly | QIODevice::Unbuffered );
	connect( d_imp, SIGNAL( bytesWritten ( qint64 ) ), this, SLOT( onBytesWritten ( qint64 ) ) );
	connect( d_imp, SIGNAL( readyRead ( ) ), this, SLOT( onReadyRead ( ) ) );
}

DbStream::~DbStream()
{
	close();
}

Transaction* DbStream::getTxn() const
{
	return static_cast<Transaction*>( parent() );
}

bool DbStream::atEnd () const { return d_imp->atEnd(); }
qint64 DbStream::bytesAvailable () const { return d_imp->bytesAvailable(); }
qint64 DbStream::bytesToWrite () const { return d_imp->bytesToWrite(); }
bool DbStream::canReadLine () const { return d_imp->canReadLine(); }
void DbStream::close ()
{
	if( !isOpen() )
		return;
	QIODevice::close();
	d_imp->close();
	getTxn()->unlockStream( d_sid );
}
bool DbStream::isSequential() const { return d_imp->isSequential(); }
qint64 DbStream::pos () const { return d_imp->pos(); }
bool DbStream::reset () { return d_imp->reset(); }
bool DbStream::seek ( qint64 pos ) { return d_imp->seek( pos ); }
qint64 DbStream::size () const { return d_imp->size(); }
bool DbStream::waitForBytesWritten ( int msecs ) { return d_imp->waitForBytesWritten( msecs ); }
bool DbStream::waitForReadyRead ( int msecs ) { return d_imp->waitForReadyRead( msecs ); }

qint64 DbStream::readData ( char * data, qint64 maxSize )
{
	return d_imp->read( data, maxSize );
}

qint64 DbStream::writeData ( const char * data, qint64 maxSize )
{
	return d_imp->write( data, maxSize );
}

void DbStream::onBytesWritten ( qint64 bytes )
{
	emit bytesWritten ( bytes ) ;
}

void DbStream::onReadyRead ()
{
	emit readyRead ();
}

void DbStream::erase()
{
	// TODO
}

StreamMeta DbStream::getMeta() const
{
	if( !isOpen() )
		return StreamMeta();
	StreamMeta m;
	getTxn()->loadStreamMeta( d_sid, m );
	return m;
}

void DbStream::copyFrom( QIODevice* in )
{
	assert( in );
	static const int maxLen = 0xffff; // RISK
	char buffer[maxLen]; 
	qint64 read = 1;
	in->seek(0);
	while( read )
	{
		read = in->read( buffer, maxLen );
		if( read > 0 )
			write( buffer, read );
	}
}

void DbStream::copyTo( QIODevice* out )
{
	assert( out );
	static const int maxLen = 0xffff; // RISK
	char buffer[maxLen]; 
	qint64 r = 1;
	seek(0);
	while( r )
	{
		r = read( buffer, maxLen );
		if( r > 0 )
		{
			if( out->write( buffer, r ) <= 0 )
				return;
		}
	}
}
