#ifndef __Sdb_Record__
#define __Sdb_Record__

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

#include <QMap>
#include <QSet>
#include <Stream/DataCell.h>
#include <Sdb/Globals.h>

namespace Sdb
{
	class Transaction;
	class Database;

	class Record 
	{
	public:
		enum Type 
		{
			TypeUndefined = 0,
			TypeObject = 1,
			TypeRelation = 2,
			TypeElement = 3
		};
		enum ReservedFields // Atom
		{
			MinReservedField = 0xffffffff - 100,
			// Object:
			FieldOwner,
			FieldPrevObj, FieldNextObj, // Liste der Aggregierten Objekte, wenn FieldOwner nicht null
			FieldFirstObj, FieldLastObj, // Liste der im Aggregat enthaltenen Objekte, deren Owner this ist.
			FieldFirstRel, FieldLastRel, // Relationsliste, alle Relationen, wo Object Source oder Target ist
			FieldFirstElm, FieldLastElm, // Elementeliste des Objekts
			// Relation:
			FieldSource, FieldTarget,
			FieldPrevSource, FieldNextSource,
			FieldPrevTarget, FieldNextTarget,
			// Element:
			FieldList, // Das Objekt, welches die Elemente enthält
			FieldValue, // *Value* Der im Element gespeicherte Wert
			FieldPrevElem, FieldNextElem,
			// Mixed
			FieldType, // *Value* Der Typename von Object und Relation als uint32
			FieldUuid // *Value* Die optionale Uuid von Object und Relation
		};
		Record();
		virtual ~Record() {}

		static const Stream::DataCell& getNull(); 

		virtual OID getId() const = 0;
		virtual Type getType() const { return TypeUndefined; }
		virtual void addRef() {}
		virtual void release() {}
		virtual bool isDeleted() const { return false; }

		typedef QMap<Atom,Stream::DataCell> Fields;
		typedef QSet<Atom> UsedFields;
		virtual UsedFields getUsedFields() const = 0;
	};

}

#endif
