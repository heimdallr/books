#include <QAbstractItemModel>
#include <QPointer>

#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "models/RoleBase.h"

#include "util/executor.h"

#include "NavigationModelController.h"

namespace HomeCompa::Flibrary {

namespace {

using Role = RoleBase;

constexpr auto AUTHORS_QUERY = "select AuthorID, FirstName, LastName, MiddleName from Authors order by LastName || FirstName || MiddleName";

void AppendTitle(QString & title, std::string_view str)
{
	if (!str.empty())
		title.append(" ").append(str.data());
}

QString CreateAuthorTitle(const DB::Query & query)
{
	QString title = query.Get<const char *>(2);
	AppendTitle(title, query.Get<const char *>(1));
	AppendTitle(title, query.Get<const char *>(3));
	return title;
}

NavigationItems CreateAuthors(DB::Database & db)
{
	NavigationItems items;
	const auto query = db.CreateQuery(AUTHORS_QUERY);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		items.emplace_back();
		items.back().Id = query->Get<long long int>(0);
		items.back().Title = CreateAuthorTitle(*query);
	}

	return items;
}

using NavigationItemsCreator = NavigationItems(*)(DB::Database & db);

void SelectDataImpl(Util::Executor & executor, DB::Database & db, NavigationItemsCreator creator, QPointer<QAbstractItemModel> model, NavigationItems & navigationItems)
{
	executor([&db, &navigationItems, model = std::move(model), creator]() mutable
	{
		auto items = creator(db);
		return[&navigationItems, items = std::move(items), model = std::move(model)]() mutable
		{
			if (model.isNull())
				return;

			(void)model->setData({}, true, Role::ResetBegin);
			navigationItems = std::move(items);
			(void)model->setData({}, true, Role::ResetEnd);
		};
	});
}

}

QAbstractItemModel * CreateNavigationListModel(NavigationItems & items, QObject * parent = nullptr);

struct NavigationModelController::Impl
{
	NavigationItems authors;
	Impl(Util::Executor & executor, DB::Database & db)
		: m_executor(executor)
		, m_db(db)
	{
	}

	void SelectData(NavigationItemsCreator creator, QPointer<QAbstractItemModel> model)
	{
		SelectDataImpl(m_executor, m_db, creator, std::move(model), authors);
	}

private:
	Util::Executor & m_executor;
	DB::Database & m_db;
};

NavigationModelController::NavigationModelController(Util::Executor & executor, DB::Database & db)
	: m_impl(executor, db)
{
}

NavigationModelController::~NavigationModelController() = default;

ModelController::Type NavigationModelController::GetType() const noexcept
{
	return Type::Navigation;
}

QAbstractItemModel * NavigationModelController::GetModelImpl(const QString & modelType)
{
	if (modelType == "Authors")
	{
		auto * model = CreateNavigationListModel(m_impl->authors);
		m_impl->SelectData(&CreateAuthors, model);
		return model;
	}

	assert(false && "unexpected modelType");
	return nullptr;
}

}
