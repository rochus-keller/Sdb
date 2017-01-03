#ifndef __Sdb_Idx__
#define __Sdb_Idx__

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
#include <Sdb/Globals.h>

namespace Sdb
{
	class Transaction;

	class Idx // Value
	{
	public:
		Idx( Transaction* = 0, int idx = 0 );
		Idx( const Idx& );

		bool first();
		bool last();
		bool firstKey();
		bool seek( const Stream::DataCell& key );
		bool seek( const QList<Stream::DataCell>& keys );
		bool gotoCur( const QByteArray& );
		bool next();
		bool nextKey();
		bool prev();
		bool prevKey();
		OID getId();

		bool isNull() const { return d_idx == 0; }
		Transaction* getTxn() const { return d_txn; }
		const QByteArray& getCur() const { return d_cur; }
		const QByteArray& getKey() const { return d_key; }
		Idx& operator=( const Idx& r );
	protected:
		void checkNull() const;
		static void addElement( QByteArray&, const IndexMeta::Item&, const Stream::DataCell& );
		static void collate( QByteArray&, quint8 collation, const QString& );
	private:
		friend class Transaction;
		// NOTE: Hier würde Database genügen. Da aber alle Txn benötigen, 
		// wird hier Txn-Pointer gespeichert
		Transaction* d_txn;
		int d_idx;
		QByteArray d_cur;
		QByteArray d_key;
	};
}

#endif
