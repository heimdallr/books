#pragma once

#include <QVariant>

#include "util/IExecutor.h"

#include "export/flint.h"

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
		DisabledBookFields,
	};

public:
	static constexpr auto SELECT_LAST_ID_QUERY = "select last_insert_rowid()";

	FLINT_EXPORT static QString GetDatabaseVersionStatement();

public:
	virtual ~IDatabaseUser() = default;

public:
	virtual size_t                           Execute(Util::IExecutor::Task&& task, int priority = 0) const = 0;
	virtual std::shared_ptr<DB::IDatabase>   Database() const                                              = 0;
	virtual std::shared_ptr<DB::IDatabase>   CheckDatabase() const                                         = 0;
	virtual std::shared_ptr<Util::IExecutor> Executor() const                                              = 0;
	virtual void                             EnableApplicationCursorChange(bool value)                     = 0;

	virtual QVariant GetSetting(Key key, QVariant defaultValue = {}) const = 0;
	virtual void     SetSetting(Key key, const QVariant& value) const      = 0;
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
