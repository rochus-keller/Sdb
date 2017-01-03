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

#ifndef __Sdb_UpdateInfo__
#define __Sdb_UpdateInfo__

#include <QMetaType>

namespace Sdb
{
	struct UpdateInfo
	{
		enum Kind
		{
			ObjectCreated,	// id=oid, name=type, eigentlich uninteressant
			ValueChanged,	// id=orl, name=field, für Orl::setValue/setType/setGuid
			RelationAdded,	// id=rid, id2=oid, name=type, where=first/last, side
			RelationMoved,	// id=rid, id2=to_rid, side, where=before/first/last
			RelationErased, // id=rid
			Aggregated,		// id=object id2=aggregate where=Last
			Deaggregated,	// id=object id2=aggregate (kommt nicht mehr bei ObjectErased)
			AggregateMoved,	// id=object, id2=to_object, where=before/first/last
			ElementAdded,	// id=eid, id2=oid, where=first/last/null
			ElementChanged, // id=eid
			ElementMoved,	// id=eid, id2=to_eid, where=before/first/last
			ElementErased,	// id=eid
			ObjectErased,	// id=oid, name=type
			StreamChanged,	// id=sid
			QueueAdded,		// id=nr, id2=oid
			QueueChanged,	// id=nr, id2=oid
			QueueErased,	// id=nr, id2=oid
			DbClosing,	
		};
		enum Where
		{
			First,
			Last,
			Before,
			After,
		};
		enum Side
		{
			Source,
			Target,
		};
		quint8 d_kind;
		quint8 d_where;
		quint8 d_side;
		quint32 d_name;
		quint64 d_id;
		quint64 d_id2;
		UpdateInfo(quint8 k = 0):d_kind(k),d_where(0),d_side(0),d_name(0),d_id(0),d_id2(0){}
	};
}
Q_DECLARE_METATYPE(Sdb::UpdateInfo)

#endif
