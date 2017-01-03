#ifndef __Sdb_Globals__
#define __Sdb_Globals__


#include <QDateTime>
#include <Stream/DataCell.h>
#include <QList>

namespace Sdb
{
	typedef Stream::DataCell::Atom Atom;
	typedef Stream::DataCell::OID OID;
	typedef quint32 Index;

	struct StreamMeta
	{
		StreamMeta():d_useCount(0),d_gzipped(false),
			d_crypted(false){}

		// Explicit Fields
		QByteArray d_mimeType; // optional, z.B. application/x-bml
		QByteArray d_encoding; // optional, z.B. UTF-8, gem. http://www.iana.org/assignments/character-sets
		QByteArray d_locale; // optional, a string of the form "language_country", where language is a lowercase, two-letter ISO 639 language code, and country is an uppercase, two-letter ISO 3166 country code.
		QByteArray d_suffix; // optional, z.B. "pdf", "doc", etc.
		bool d_gzipped;
		bool d_crypted;
		// Automatic Fields
		QDateTime d_lastUse;
		quint32 d_useCount;
	};

	struct IndexMeta
	{
		// Die Enums sind persistent. Vorsicht bei Änderung.
		enum Kind 
		{ 
			Value = 1,	// Mehrere Items zulässig. Diese werden einfach binär hintereinandergefügt.
			Unique = 2, // Value, aber Wert der Items wird ohne nachgestellte ID gespeichert. Nur für Value
			Fulltext = 3, // Nur ein Item zulässig. Dieses wird in Wörter aufgespalten, falls Text.
		};
		Kind d_kind;

		enum Collation 
		{ 
			None = 0, // Ergebnis UTF-8
			NFKD_CanonicalBase = 1, // Mache QChar::decompose und verwende bei QChar::Canonical nur Basiszeichen für Vergleich, sonst alles, Ergebnis UTF-8
		};

		struct Item
		{
			quint32 d_atom; // Feld, das indiziert wird
			bool d_nocase; // true..transformiere in Kleinbuchstaben vor Vergleich (falls Text)
			bool d_invert; // true..invertiere Daten so dass absteigend sortiert wird

			quint8 d_coll; // Collation

			Item( quint32 atom = 0, Collation c = None, bool nc = true, bool inv = false ):
				d_atom(atom),d_nocase(nc),d_invert(inv),d_coll(c) {}
		};
		QList<Item> d_items;

		IndexMeta(Kind k = Value):d_kind(k) {}
	};
}

#endif
