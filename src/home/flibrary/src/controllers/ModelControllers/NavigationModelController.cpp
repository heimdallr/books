#include <QAbstractItemModel>
#include <QPointer>

#include "fnd/FindPair.h"

#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "models/RoleBase.h"

#include "util/executor.h"

#include "NavigationModelController.h"

namespace HomeCompa::Flibrary {

namespace {

using Role = RoleBase;

constexpr auto AUTHORS_QUERY = "select AuthorID, FirstName, LastName, MiddleName from Authors order by LastName || FirstName || MiddleName";
constexpr auto SERIES_QUERY = "select SeriesID, SeriesTitle from Series order by SeriesTitle";
constexpr auto GENRES_QUERY = "select GenreCode, ParentCode, GenreAlias from Genres";

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
		items.back().Id = QString::number(query->Get<int>(0));
		items.back().Title = CreateAuthorTitle(*query);
	}

	return items;
}

NavigationItems CreateSeries(DB::Database & db)
{
	NavigationItems items;
	const auto query = db.CreateQuery(SERIES_QUERY);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		items.emplace_back();
		items.back().Id = QString::number(query->Get<int>(0));
		items.back().Title = query->Get<const char *>(1);
	}

	return items;
}

NavigationItems CreateGenres(DB::Database & db)
{
	NavigationItems items;
	const auto query = db.CreateQuery(GENRES_QUERY);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		items.emplace_back();
		items.back().Id = query->Get<const char *>(0);
		items.back().Title = query->Get<const char *>(2);
	}

	return items;
}

using NavigationItemsCreator = NavigationItems(*)(DB::Database & db);

constexpr std::pair<const char *, NavigationItemsCreator> g_creators[]
{
#define ITEM(NAME) { #NAME, &Create##NAME }
		ITEM(Authors),
		ITEM(Series),
		ITEM(Genres),
#undef	ITEM
};

}

QAbstractItemModel * CreateNavigationListModel(NavigationItems & items, QObject * parent = nullptr);

class NavigationModelController::Impl
{
public:
	Impl(Util::Executor & executor, DB::Database & db)
		: m_executor(executor)
		, m_db(db)
	{
	}

	QAbstractItemModel * GetModel(const QString & modelType)
	{
		const auto & creator = FindSecond(g_creators, modelType.toStdString().data(), PszComparer {});
		return GetModelImpl(creator, m_items);
	}

private:
	QAbstractItemModel * GetModelImpl(NavigationItemsCreator creator, NavigationItems & navigationItems) const
	{
		auto * model = CreateNavigationListModel(navigationItems);
		m_executor([&, model = QPointer(model), creator]() mutable
		{
			auto items = creator(m_db);
			return[&, items = std::move(items), model = std::move(model)]() mutable
			{
				if (model.isNull())
					return;

				(void)model->setData({}, true, Role::ResetBegin);
				navigationItems = std::move(items);
				(void)model->setData({}, true, Role::ResetEnd);
			};
		}, 1);
		return model;
	}

private:
	Util::Executor & m_executor;
	DB::Database & m_db;
	NavigationItems m_items;
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
	return m_impl->GetModel(modelType);
}

}
