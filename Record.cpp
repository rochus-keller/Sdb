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

#include "Record.h"
#include "Exceptions.h"
#include <Stream/DataWriter.h>
#include <Stream/DataReader.h>
#include <Stream/Helper.h>
#include <QIODevice>
#include <cassert>
using namespace Sdb;
using namespace Stream;

Record::Record()
{
}

const Stream::DataCell& Record::getNull()
{
	static const DataCell dummy = DataCell().setNull();
	return dummy;
}
