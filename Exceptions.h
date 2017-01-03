#ifndef __Sdb_Exceptions__
#define __Sdb_Exceptions__

#include <exception>
#include <QString>

namespace Sdb
{
	class DatabaseException : public std::exception
	{
	public:
		enum Code {
			OpenDbFile,
			StartTrans,
			CommitTrans,
			AccessMeta,
			CreateBtCursor,
			CreateTable,
			RemoveTable,
			ClearTable,
			AccessCursor,
			AccessDatabase,
			DatabaseMeta,
			RecordFormat,
			UnknownId,
			AccessRecord,
			ReservedName,
			WrongType,
			NotInTransaction,
			RecordLocked,
			RecordDeleted,
			AtomClash,
			WrongContext,
			InvalidArgument,
			SelfRelation,
			StreamsDir,
			StreamFile,
			IndexExists,
			Duplicate,
			// Strigliste anpassen
		};
		DatabaseException( Code c, const QString& msg = "" ):d_err(c),d_msg(msg){}
		virtual ~DatabaseException() throw() {}
		Code getCode() const { return d_err; }
		QString getCodeString() const;
		const QString& getMsg() const { return d_msg; }
	private:
		Code d_err;
		QString d_msg;
	};

}

#endif
