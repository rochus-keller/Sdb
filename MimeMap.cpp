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

#include "MimeMap.h"
using namespace Sdb;
using namespace Stream;

QByteArray MimeMap::suffixToMimeType( const QByteArray& suffix )
{
	// TODO: effizienter z.B. mit Quicksort
	const QByteArray s = suffix.toLower();
	int i = 0;
	while( true )
	{
		if( s_slots[i].d_fileSuffix == 0 )
			break;
		if( s == s_slots[i].d_fileSuffix && s_slots[i].d_primary )
			return s_slots[i].d_mimeType;
		i++;
	}
	return QByteArray();
}

QByteArray MimeMap::mimeTypeToSuffix( const QByteArray& type  )
{
	const QByteArray t = type.toLower();
	int i = 0;
	while( true )
	{
		if( s_slots[i].d_fileSuffix == 0 )
			break;
		if( t == s_slots[i].d_mimeType && s_slots[i].d_primary )
			return s_slots[i].d_fileSuffix;
		i++;
	}
	return QByteArray();
}

const MimeMap::Slot MimeMap::s_slots[] =
{
	{ "aif", "audio/aiff", DataCell::TypeLob, true },
	{ "avi", "video/avi", DataCell::TypeLob, true },
	{ "bmp", "image/bmp", DataCell::TypeImg, true },
	{ "bz", "application/x-bzip", DataCell::TypeLob, true },
	{ "bz2", "application/x-bzip2", DataCell::TypeLob, true },
	{ "css", "text/css", DataCell::TypeLob, true },
	{ "doc", "application/msword", DataCell::TypeLob, true },
	{ "dot", "application/msword", DataCell::TypeLob, false },
	{ "dvi", "application/x-dvi", DataCell::TypeLob, true },
	{ "eml", "message/rfc822", DataCell::TypeLob, true },
	{ "eps", "application/postscript", DataCell::TypeLob, false },
	{ "exe", "application/octet-stream", DataCell::TypeLob, false },
	{ "gif", "image/gif", DataCell::TypeImg, true },
	{ "gz", "application/x-compressed", DataCell::TypeLob, false },
	{ "gz", "application/x-gzip", DataCell::TypeLob, true },
	{ "gzip", "application/x-gzip", DataCell::TypeLob, false },
	{ "gzip", "multipart/x-gzip", DataCell::TypeLob, false },
	{ "hqx", "application/binhex", DataCell::TypeLob, true },
	{ "htm", "text/html", DataCell::TypeHtml, false },
	{ "html", "text/html", DataCell::TypeHtml, true },
	{ "ico", "image/x-icon", DataCell::TypeLob, true },
	{ "jpeg", "image/jpeg", DataCell::TypeImg, false },
	{ "jpg", "image/jpeg", DataCell::TypeImg, true },
	{ "latex", "application/x-latex", DataCell::TypeLob, true },
	{ "mht", "message/rfc822", DataCell::TypeLob, false },
	{ "mid", "audio/midi", DataCell::TypeLob, true },
	{ "midi", "audio/midi", DataCell::TypeLob, false },
	{ "mp3", "audio/mpeg3", DataCell::TypeLob, true },
	{ "mpg", "video/mpeg", DataCell::TypeLob, true },
	{ "mpp", "application/vnd.ms-project", DataCell::TypeLob, true },
	{ "pct", "image/x-pict", DataCell::TypeLob, true },
	{ "pdf", "application/pdf", DataCell::TypeLob, true },
	{ "pic", "image/pict", DataCell::TypeLob, true },
	{ "png", "image/png", DataCell::TypeImg, true },
	{ "ppt", "application/vnd.ms-powerpoint", DataCell::TypeLob, true },
	{ "ps", "application/postscript", DataCell::TypeLob, true },
	{ "qt", "video/quicktime", DataCell::TypeLob, true },
	{ "ra", "audio/x-pn-realaudio", DataCell::TypeLob, false },
	{ "ram", "audio/x-pn-realaudio", DataCell::TypeLob, true },
	{ "rm", "application/vnd.rn-realmedia", DataCell::TypeLob, true },
	{ "rtf", "application/rtf", DataCell::TypeLob, true },
	{ "sgm", "text/sgml", DataCell::TypeLob, true },
	{ "sgml", "text/sgml", DataCell::TypeLob, true },
	{ "tar", "application/x-tar", DataCell::TypeLob, true },
	{ "tgz", "application/gnutar", DataCell::TypeLob, true },
	{ "tif", "image/tiff", DataCell::TypeImg, true },
	{ "tiff", "image/tiff", DataCell::TypeImg, false },
	{ "txt", "text/plain", DataCell::TypeString, true },
	{ "vrml", "application/x-vrml", DataCell::TypeLob, true },
	{ "vsd", "application/x-visio", DataCell::TypeLob, true },
	{ "wav", "audio/wav", DataCell::TypeLob, true },
	{ "wmf", "windows/metafile", DataCell::TypeLob, true },
	{ "wri", "application/mswrite", DataCell::TypeLob, true },
	{ "xls", "application/vnd.ms-excel", DataCell::TypeLob, true },
	{ "xml", "text/xml", DataCell::TypeXml, true },
	{ "zip", "application/zip", DataCell::TypeLob, true },
	{ 0, 0, DataCell::TypeInvalid, false },
};
