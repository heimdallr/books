#pragma once

#include <QVariant>

#include "util/IExecutor.h"

namespace HomeCompa::DB
{
class IDatabase;
class IQuery;
}

namespace HomeCompa::Flibrary
{

class IDatabaseUser // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	enum class Key
	{
		DatabaseVersion,
	};

public:
	static constexpr auto BOOKS_QUERY_FIELDS = "b.BookID, b.Title, coalesce(%1.SeqNumber, -1), b.UpdateDate, b.LibRate, b.Lang, f.FolderTitle, b.FileName || b.Ext, b.BookSize, coalesce(bu.userRate, 0), "
											   "coalesce(bu.IsDeleted, b.IsDeleted, 0), b.FolderID, b.UpdateID";
	static constexpr auto SELECT_LAST_ID_QUERY = "select last_insert_rowid()";

public:
	virtual ~IDatabaseUser() = default;

public:
	virtual size_t Execute(Util::IExecutor::Task&& task, int priority = 0) const = 0;
	virtual std::shared_ptr<DB::IDatabase> Database() const = 0;
	virtual std::shared_ptr<DB::IDatabase> CheckDatabase() const = 0;
	virtual std::shared_ptr<Util::IExecutor> Executor() const = 0;
	virtual void EnableApplicationCursorChange(bool value) = 0;

	virtual QVariant GetSetting(Key key, QVariant defaultValue = {}) const = 0;
	virtual void SetSetting(Key key, const QVariant& value) const = 0;
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
		FolderID,
		UpdateID,
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

} // namespace HomeCompa::Flibrary
