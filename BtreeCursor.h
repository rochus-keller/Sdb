#ifndef __Sdb_Btree__
#define __Sdb_Btree__

#include <QObject>

struct BtCursor;

namespace Sdb
{
	class BtreeStore;

	// Interne Klasse
	// Ein BtreeCursor repräsentiert einen Sqlite-Btree-Table mit einem Qt-kompatiblen Interface

	class BtreeCursor // Value
	{
	public:
		BtreeCursor();
		~BtreeCursor();

		void open( BtreeStore*, int table, bool writing = false );
		void close();

		// Navigation
		bool moveFirst(); // false..empty table
		bool moveLast();  // false..empty table
		bool moveNext();  // false..passed last entry, not pointing on valid entry anymore
		bool movePrev();  // false..passed first entry, not pointing on valid entry anymore
		bool moveTo( const QByteArray& key, bool partial = false ); // partial=true: true..pos beginnt mit key
		bool moveNext( const QByteArray& key );  // partial, false..passed key or last entry
		bool isValidPos(); 

		// moveTo (egal ob partial oder nicht):
		// bei false ist Position entweder auf nächst grösserem Wert als key oder über
		// das Ende hinaus, bzw. !isValidPos.

		// Read/Write
		void insert( const QByteArray& key, const QByteArray& value ); // Unabhängig von Pos
		QByteArray readKey() const; // Pos
		QByteArray readValue() const; // Pos
		void remove(); // Pos

		BtreeStore* getDb() const { return d_db; }
	protected:
		void checkOpen() const;
	private:
		int d_table;
		BtreeStore* d_db;
		BtCursor* d_cur;
	};
}

#endif