#pragma once

#include "util/IExecutor.h"

namespace HomeCompa::DB {
class IDatabase;
class IQuery;
}

namespace HomeCompa::Flibrary {

class IDatabaseUser  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	static constexpr auto BOOKS_QUERY_FIELDS = "b.BookID, b.Title, coalesce(b.SeqNumber, -1), b.UpdateDate, b.LibRate, b.Lang, b.Folder, b.FileName || b.Ext, b.BookSize, coalesce(bu.userRate, 0), coalesce(bu.IsDeleted, b.IsDeleted, 0)";
	static constexpr auto SELECT_LAST_ID_QUERY = "select last_insert_rowid()";

public:
	virtual ~IDatabaseUser() = default;

public:
	virtual size_t Execute(Util::IExecutor::Task && task, int priority = 0) const = 0;
	virtual std::shared_ptr<DB::IDatabase> Database() const = 0;
	virtual std::shared_ptr<DB::IDatabase> CheckDatabase() const = 0;
	virtual std::shared_ptr<Util::IExecutor> Executor() const = 0;
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
		UserRate,
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
