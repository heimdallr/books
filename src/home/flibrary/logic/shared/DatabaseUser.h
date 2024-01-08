#pragma once

#include <QTimer>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IDataItem.h"
#include "interface/logic/ILogicFactory.h"

#include "util/IExecutor.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

namespace HomeCompa::Flibrary {

class DatabaseUser
{
	NON_COPY_MOVABLE(DatabaseUser)
public:
	static constexpr auto BOOKS_QUERY_FIELDS = "b.BookID, b.Title, coalesce(b.SeqNumber, -1), b.UpdateDate, b.LibRate, b.Lang, b.Folder, b.FileName || b.Ext, b.BookSize, coalesce(bu.IsDeleted, b.IsDeleted, 0)";
	static constexpr auto SELECT_LAST_ID_QUERY = "select last_insert_rowid()";

public:
	DatabaseUser(const std::shared_ptr<ILogicFactory> & logicFactory
		, std::shared_ptr<class DatabaseController> databaseController
	);
	~DatabaseUser();

public:
	size_t Execute(Util::IExecutor::Task && task, int priority = 0) const;
	std::shared_ptr<DB::IDatabase> Database() const;
	std::shared_ptr<Util::IExecutor> Executor() const;

public:
	static IDataItem::Ptr CreateSimpleListItem(const DB::IQuery & query, const int * index);
	static IDataItem::Ptr CreateGenreItem(const DB::IQuery & query, const int * index);
	static IDataItem::Ptr CreateFullAuthorItem(const DB::IQuery & query, const int * index);
	static IDataItem::Ptr CreateBookItem(const DB::IQuery & query);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

struct BookQueryFields
{
	enum Value
	{
		BookId = 0,
		BookTitle,
		SeqNumber,
		UpdateDate,
		LibRate,
		Lang,
		Folder,
		FileName,
		Size,
		IsDeleted,
		AuthorId,
		AuthorLastName,
		AuthorFirstName,
		AuthorMiddleName,
		GenreCode,
		GenreTitle,
		GenreFB2Code,
		SeriesId,
		SeriesTitle,
	};
};

}
