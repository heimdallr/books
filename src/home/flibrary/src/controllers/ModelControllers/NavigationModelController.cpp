#include <QAbstractItemModel>

#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "models/RoleBase.h"

#include "util/executor.h"

#include "NavigationModelController.h"

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

NavigationItems CreateItems(DB::Database & db)
{
	NavigationItems items;
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

QAbstractItemModel * CreateAuthorsModel(NavigationItems & items, QObject * parent = nullptr);

struct NavigationModelController::Impl
{
	Impl(NavigationModelController & self, Util::Executor & executor, DB::Database & db)
		: m_self(self)
		, m_executor(executor)
		, m_db(db)
	{
		m_executor([&]
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
	NavigationModelController & m_self;
	Util::Executor & m_executor;
	DB::Database & m_db;
};

NavigationModelController::NavigationModelController(Util::Executor & executor, DB::Database & db)
	: ModelController(CreateAuthorsModel(m_authors))
	, m_impl(*this, executor, db)
{
}

NavigationModelController::~NavigationModelController() = default;

ModelController::Type NavigationModelController::GetType() const noexcept
{
	return Type::Authors;
}

}
