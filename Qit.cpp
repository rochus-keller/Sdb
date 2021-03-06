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

#include "Qit.h"
#include "Record.h"
#include "Exceptions.h"
#include "Transaction.h"
#include "Database.h"
#include <Sdb/BtreeCursor.h>
#include <Stream/DataCell.h>
#include <Stream/BmlRecord.h>
using namespace Sdb;
using namespace Stream;

Qit::Qit( Record* rec, Transaction* txn, quint32 nr )
{
	d_rec = rec;
	d_txn = txn;
	d_nr = nr;
	if( d_rec )
		d_rec->addRef();
}

Qit::Qit( const Qit& lhs )
{
	d_rec = 0;
	d_txn = 0;
	d_nr = 0;
	*this = lhs;
}

Qit::~Qit()
{
	if( d_rec )
		d_rec->release();
}

Qit& Qit::assign( const Qit& r )
{
	if( d_rec == r.d_rec && d_nr == r.d_nr )
		return *this;
	if( d_rec )
		d_rec->release();
	d_rec = r.d_rec;
	d_txn = r.d_txn;
	d_nr = r.d_nr;
	if( d_rec )
		d_rec->addRef();
	return *this;
}	

void Qit::checkNull() const
{
	if( d_rec == 0 )
		throw DatabaseException(DatabaseException::AccessRecord, "null");
}

void Qit::setValue( const Stream::DataCell& cell )
{
	checkNull();
	if( d_nr == 0 )
		return; // RISK
	d_txn->setQSlot( d_rec, d_nr, cell );
	UpdateInfo c;
	c.d_kind = (cell.isNull())?UpdateInfo::QueueErased:UpdateInfo::ValueChanged;
	c.d_id = d_nr;
	c.d_id2 = d_rec->getId();
	d_txn->d_notify.append( c );
}

void Qit::getValue( Stream::DataCell& v ) const
{
	checkNull();
	if( d_nr == 0 )
		v.setNull();
	else
		d_txn->getQSlot( d_rec, d_nr, v );
}

Stream::DataCell Qit::getValue() const
{
	Stream::DataCell v;
	getValue( v );
	return v;
}

bool Qit::first()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getQueTable() );
	const QByteArray oid = DataCell().setId64( d_rec->getId() ).writeCell();
	if( !cur.moveTo( oid ) )
		return false;
	if( !cur.moveNext() )
		return false;
	const QByteArray key = cur.readKey();
	if( !key.startsWith( oid ) )
		return false;
	DataCell v;
	v.readCell( key.mid( oid.length() ) );
	d_nr = v.getId32();
	return true;
}

bool Qit::last()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getQueTable() );
	const QByteArray oid = DataCell().setId64( d_rec->getId() ).writeCell();
	DataCell v;
	d_txn->getQSlot( d_rec, 0, v );
	const QByteArray nr = v.writeCell();
	QByteArray key;
	if( !cur.moveTo( oid + nr, true ) ) // partial, damit richtig positioniert
	{
		// cur steht auf Ende oder Wert nach oid + nr
		if( cur.isValidPos() )
		{
			// Wenn auf n�chst gr�sserem Wert nach oid + nr, dann eins zur�ck
			if( !cur.movePrev() ) 
				return false; // wir sind am Anfang, offensichtlich gibt es oid nicht in Queue
		}else
			cur.moveLast(); // Korrigiere wieder auf letzten
		key = cur.readKey();
		if( !key.startsWith( oid ) || key == oid )
			return false;
	}else
		key = cur.readKey();
	v.readCell( key.mid( oid.length() ) );
	d_nr = v.getId32();
	return true;
}

bool Qit::next()
{
	if( d_nr == 0 )
		return first();
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getQueTable() );
	const QByteArray oid = DataCell().setId64( d_rec->getId() ).writeCell();
	const QByteArray nr = DataCell().setId32( d_nr ).writeCell();
	if( cur.moveTo( oid + nr ) )
		cur.moveNext();
	if( !cur.readKey().startsWith( oid ) )
		return false;
	DataCell v;
	v.readCell( cur.readKey().mid( oid.length() ) );
	d_nr = v.getId32();
	return true;
}

bool Qit::prev()
{
	if( d_nr == 0 )
		return last();
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getQueTable() );
	const QByteArray oid = DataCell().setId64( d_rec->getId() ).writeCell();
	const QByteArray nr = DataCell().setId32( d_nr ).writeCell();
	if( cur.moveTo( oid + nr ) )
		cur.movePrev();
	const QByteArray key = cur.readKey();
	if( key == oid || !key.startsWith( oid ) )
		return false;
	DataCell v;
	v.readCell( cur.readKey().mid( oid.length() ) );
	d_nr = v.getId32();
	return true;
}

void Qit::erase()
{
	DataCell v;
	v.setNull();
	setValue( v );
}
