#ifndef _Sdb_Mit_
#define _Sdb_Mit_

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


#include <Stream/DataCell.h>
#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>


namespace Sdb
{
	class Record;
	class Transaction;
	class Database;

	class Mit
	{
	public:
		Mit( Record* = 0, Transaction* = 0 );
		Mit( const Mit& );
		~Mit();

		bool seek( const QList<Stream::DataCell>& keys );
		void getValue( Stream::DataCell& ) const;
		Stream::DataCell getValue() const;
		QList<Stream::DataCell> getKey() const;
		bool firstKey();
		bool nextKey(); // false..kein Nachfolger, unverändert
		bool prevKey(); // false..kein Vorgänger, unverängert

		Mit& operator=( const Mit& r ) { return assign( r ); }
		Mit& assign( const Mit& r );
		Record* getRec() const { return d_rec; }
		Transaction* getTxn() const { return d_txn; }
		bool isNull() const { return d_rec == 0; }
	protected:
		void checkNull() const;
		Record* d_rec;
		Transaction* d_txn;
		QByteArray d_cur;
		QByteArray d_key;
	};
}

#endif
