#include <QAbstractItemModel>

#include "fnd/executor.h"

#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "models/RoleBase.h"

#include "AuthorsModelController.h"

namespace HomeCompa::Flibrary {

namespace {

using Role = RoleBase;

constexpr auto QUERY = "select AuthorID, FirstName, LastName, MiddleName from Authors order by LastName || FirstName || MiddleName";

void AppendTitle(QString & title, std::string_view str)
{
	if (!str.empty())
		title.append(" ").append(str.data());
}

QString CreateTitle(const DB::Query & query)
{
	QString title = query.Get<const char *>(2);
	AppendTitle(title, query.Get<const char *>(1));
	AppendTitle(title, query.Get<const char *>(3));
	return title;
}

Authors CreateItems(DB::Database & db)
{
	Authors items;
	const auto query = db.CreateQuery(QUERY);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		items.emplace_back();
		items.back().Id = query->Get<long long int>(0);
		items.back().Title = CreateTitle(*query);
	}

	return items;
}
}

QAbstractItemModel * CreateAuthorsModel(Authors & items, QObject * parent = nullptr);

struct AuthorsModelController::Impl
{
	Impl(AuthorsModelController & self, Executor & executor, DB::Database & db)
		: m_self(self)
		, m_executor(executor)
		, m_db(db)
	{
		m_executor.Execute([&]
		{
			auto items = CreateItems(m_db);
			return[&, items = std::move(items)]() mutable
			{
				(void)m_self.GetModel()->setData({}, true, Role::ResetBegin);
				m_self.m_authors = std::move(items);
				(void)m_self.GetModel()->setData({}, true, Role::ResetEnd);
			};
		});
	}

private:
	AuthorsModelController & m_self;
	Executor & m_executor;
	DB::Database & m_db;
};

AuthorsModelController::AuthorsModelController(Executor & executor, DB::Database & db)
	: ModelController(CreateAuthorsModel(m_authors))
	, m_impl(*this, executor, db)
{
}

AuthorsModelController::~AuthorsModelController() = default;

ModelController::Type AuthorsModelController::GetType() const noexcept
{
	return Type::Authors;
}

}
