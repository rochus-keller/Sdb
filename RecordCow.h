#ifndef __Sdb_RecordCow__
#define __Sdb_RecordCow__

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

#include <Sdb/Record.h>

namespace Sdb
{
	class RecordImp;
	class Transaction;

	class RecordCow : public Record
	{
	public:
		RecordCow( RecordImp*, Transaction* );
		~RecordCow();

		const QMap<quint32,Stream::DataCell>& getQueue() const { return d_queue; }
		const QMap<QByteArray,Stream::DataCell>& getMap() const { return d_map; }

		// Overrides
		const Stream::DataCell& getField( quint32 ) const;
		Type getType() const;
		OID getId() const;
		void addRef();
		void release();
		UsedFields getUsedFields() const;
		bool isDeleted() const;
	private:
		friend class Transaction;
		RecordImp* d_imp;
		Transaction* d_txn;
		int d_refCount;
		Fields d_fields; // Atom:Value
		QMap<quint32,Stream::DataCell> d_queue;
		QMap<QByteArray,Stream::DataCell> d_map; // key: BML (vector<cell>)
	};
}

#endif
