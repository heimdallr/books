#pragma once

#include <QVariant>

#include "fnd/observer.h"

#include "util/IExecutor.h"

#include "export/flint.h"

namespace HomeCompa::DB {

class IDatabase;

} // namespace HomeCompa::DB

namespace HomeCompa::Flibrary {

class IDatabaseUser // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnInsert(std::string_view dbName, std::string_view tableName, int64_t rowId) = 0;
		virtual void OnUpdate(std::string_view dbName, std::string_view tableName, int64_t rowId) = 0;
		virtual void OnDelete(std::string_view dbName, std::string_view tableName, int64_t rowId) = 0;
	};

public:
	enum class Key
	{
		DatabaseVersion,
		DisabledBookFields,
		RecentBookDateTime,
	};

public:
	static constexpr auto SELECT_LAST_ID_QUERY = "select last_insert_rowid()";

	static constexpr auto SET_USER_RATE_COMMAND = R"(
INSERT INTO Books_User (BookID, UserRate, CreatedAt) VALUES (?, ?, datetime(CURRENT_TIMESTAMP, 'localtime')) 
ON CONFLICT(BookID) DO UPDATE SET UserRate = excluded.UserRate, CreatedAt = excluded.CreatedAt)";

	static constexpr auto SET_LANG_COMMAND = R"(
INSERT INTO Books_User (Lang, BookID, CreatedAt) VALUES (?, ?, datetime(CURRENT_TIMESTAMP, 'localtime')) 
ON CONFLICT(BookID) DO UPDATE SET Lang = excluded.Lang, CreatedAt = excluded.CreatedAt)";

	FLINT_EXPORT static QString GetDatabaseVersionStatement();

public:
	virtual ~IDatabaseUser() = default;

public:
	virtual size_t                           Execute(Util::IExecutor::Task&& task, int priority = 0) const = 0;
	virtual void                             Reset()                                                       = 0;
	virtual std::shared_ptr<DB::IDatabase>   GetDatabase() const                                           = 0;
	virtual std::shared_ptr<Util::IExecutor> Executor() const                                              = 0;
	virtual void                             EnableApplicationCursorChange(bool value)                     = 0;

	virtual QVariant GetSetting(Key key, QVariant defaultValue = {}) const = 0;
	virtual void     SetSetting(Key key, const QVariant& value) const      = 0;

	virtual void RegisterObserver(IObserver* observer)   = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

struct BookQueryFields
{
	enum Value
	{
		BookId = 0,
		BookTitle,
		UpdateDate,
		LibRate,
		Lang,
		Format,
		Year,
		Folder,
		FileName,
		Size,
		UserRate,
		LibID,
		IsDeleted,
		Flags,
		Last
	};
};

} // namespace HomeCompa::Flibrary
