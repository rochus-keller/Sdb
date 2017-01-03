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

#include "RecordCow.h"
#include "RecordImp.h"
#include <cassert>
using namespace Sdb;

RecordCow::RecordCow( RecordImp* cow, Transaction* txn ):d_refCount(0)
{
	d_imp = cow;
	assert( d_imp != 0 );
	d_imp->addRef();
	d_txn = txn;
	assert( d_txn != 0 );
}

RecordCow::~RecordCow()
{
	d_imp->release();
}

const Stream::DataCell& RecordCow::getField( quint32 id ) const
{
	Fields::const_iterator i = d_fields.find( id );
	if( i == d_fields.end() )
		return d_imp->getField( id );
	else
		return i.value();
}

void RecordCow::addRef()
{
	d_refCount++;
	d_imp->addRef();
}

void RecordCow::release()
{
	d_refCount--;
	d_imp->release();
}

OID RecordCow::getId() const 
{ 
	return d_imp->getId(); 
}

Record::Type RecordCow::getType() const 
{ 
	return d_imp->getType(); 
}

RecordCow::UsedFields RecordCow::getUsedFields() const
{
	UsedFields res = d_imp->getUsedFields();
	Record::Fields::const_iterator i;
	for( i = d_fields.begin(); i != d_fields.end(); ++i )
		if( i.key() < MinReservedField )
			res.insert( i.key() );
	return res;
}

bool RecordCow::isDeleted() const
{
	if( d_imp )
		return d_imp->isDeleted();
	else
		return false;
}
