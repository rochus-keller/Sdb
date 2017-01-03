#ifndef __Sdb_CacheRecord__
#define __Sdb_CacheRecord__

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
	class RecordCow;

	class RecordImp : public Record
	{
	public:
		enum State { StateIdle, StateNew, StateToDelete, StateDeleted };

		RecordImp(Database*,OID id, Record::Type = Record::TypeUndefined );

		RecordCow* getCow() const { return d_cow; }
		Database* getDb() const { return d_db; }
		int getRefCount() const { return d_refCount; }

		void writeTo( QIODevice* ) const;
		void readFrom( QIODevice* );
		void clear();
		void dump();

		// Overrides
		const Stream::DataCell& getField( quint32 ) const;
		OID getId() const { return d_id; }
		Type getType() const { return (Type)d_type; }
		void addRef();
		void release();
		UsedFields getUsedFields() const;
		bool isDeleted() const { return d_state == StateDeleted; }
	private:
		friend class Transaction;
		quint8 d_type; // Type
		quint8 d_state;
		OID d_id; // Eindeutig über alle Record-Types hinweg
		Database* d_db;
		RecordCow* d_cow; // wenn nicht null..lock, Record wird von cow geändert
		int d_refCount;
		Fields d_fields; // Atom:Value
	};
}

#endif
