#include <stack>

#include <QAbstractItemModel>
#include <QPointer>

#include "fnd/FindPair.h"

#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "models/NavigationListItem.h"
#include "models/NavigationTreeItem.h"
#include "models/RoleBase.h"

#include "util/executor.h"

#include "NavigationModelController.h"

#include "Settings/UiSettings_keys.h"
#include "Settings/UiSettings_values.h"

namespace HomeCompa::Flibrary {

namespace {

using Role = RoleBase;

constexpr auto AUTHORS_QUERY = "select AuthorID, FirstName, LastName, MiddleName from Authors order by LastName || FirstName || MiddleName";
constexpr auto SERIES_QUERY = "select SeriesID, SeriesTitle from Series order by SeriesTitle";
constexpr auto GENRES_QUERY = "select g.GenreCode, g.ParentCode, g.GenreAlias from Genres g where exists (select 42 from Genre_List l where l.GenreCode = g.GenreCode) or exists (select 42 from Genres p where p.ParentCode = g.GenreCode)";

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

NavigationListItems CreateAuthors(DB::Database & db)
{
	NavigationListItems items;
	const auto query = db.CreateQuery(AUTHORS_QUERY);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		items.emplace_back();
		items.back().Id = QString::number(query->Get<int>(0));
		items.back().Title = CreateAuthorTitle(*query);
	}

	return items;
}

NavigationListItems CreateSeries(DB::Database & db)
{
	NavigationListItems items;
	const auto query = db.CreateQuery(SERIES_QUERY);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		items.emplace_back();
		items.back().Id = QString::number(query->Get<int>(0));
		items.back().Title = query->Get<const char *>(1);
	}

	return items;
}

namespace GenresDetails {

NavigationTreeItems SelectGenres(DB::Database & db)
{
	NavigationTreeItems items;
	const auto query = db.CreateQuery(GENRES_QUERY);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		items.emplace_back();
		items.back().Id = query->Get<const char *>(0);
		items.back().Title = query->Get<const char *>(2);
		items.back().ParentId = query->Get<const char *>(1);
	}

	return items;
}

struct S
{
	QString id;
	size_t index;
	int level;
};

NavigationTreeItems ReorderGenres(NavigationTreeItems & items)
{
	std::map<QString, std::map<int, size_t, std::greater<>>> treeIndex;
	for (size_t i = 0, sz = std::size(items); i < sz; ++i)
	{
		const auto & item = items[i];
		treeIndex[item.ParentId][item.Id.mid(item.Id.lastIndexOf('.') + 1).toInt()] = i;
	}

	for (auto & item : items)
		if (const auto it = treeIndex.find(item.Id); it != treeIndex.end())
			item.ChildrenCount = static_cast<int>(it->second.size());

	std::vector<size_t> sortedIndex;

	std::stack<S> s;
	for (const auto & [_, i] : treeIndex["0"])
	{
		s.emplace(items[i].Id, i, 0);
		items[i].ParentId.clear();
	}

	while (!s.empty())
	{
		auto [parentId, index, level] = std::move(s.top());
		sortedIndex.push_back(index);
		items[index].TreeLevel = level;
		s.pop();
		for (const auto & [_, i] : treeIndex[parentId])
			s.emplace(items[i].Id, i, level + 1);
	}

	NavigationTreeItems result;
	result.reserve(items.size());
	std::ranges::transform(std::as_const(sortedIndex), std::back_inserter(result), [&items] (size_t index) { return std::move(items[index]); });
	return result;
}

}

NavigationTreeItems CreateGenres(DB::Database & db)
{
	NavigationTreeItems items = GenresDetails::SelectGenres(db);
	return GenresDetails::ReorderGenres(items);
}

template<typename T>
using NavigationItemsCreator = std::vector<T>(*)(DB::Database & db);

}

QAbstractItemModel * CreateNavigationModel(NavigationListItems & items, QObject * parent = nullptr);
QAbstractItemModel * CreateNavigationModel(NavigationTreeItems & items, QObject * parent = nullptr);

class NavigationModelController::Impl
{
public:
	Impl(NavigationModelController & self, Util::Executor & executor, DB::Database & db, const NavigationSource navigationSource)
		: m_self(self)
		, m_executor(executor)
		, m_db(db)
		, m_navigationSource(navigationSource)
	{
	}

	QAbstractItemModel * CreateModel()
	{
		switch(m_navigationSource)
		{
			case NavigationSource::Authors:
				return CreateModelImpl<NavigationListItem>(&CreateAuthors, m_authors);
			case NavigationSource::Series:
				return CreateModelImpl<NavigationListItem>(&CreateSeries, m_series);
			case NavigationSource::Genres:
				return CreateModelImpl<NavigationTreeItem>(&CreateGenres, m_genres);
			default:
				break;
		}
		throw std::invalid_argument("unexpected model source");
	}

private:
	template<typename T>
	QAbstractItemModel * CreateModelImpl(NavigationItemsCreator<T> creator, std::vector<T> & navigationItems) const
	{
		auto * model = CreateNavigationModel(navigationItems);
		m_executor({ "Get navigation", [&, model = QPointer(model), creator]() mutable
		{
			auto items = creator(m_db);
			return[&, items = std::move(items), model = std::move(model)]() mutable
			{
				if (model.isNull())
					return;

				(void)model->setData({}, true, Role::ResetBegin);
				navigationItems = std::move(items);
				(void)model->setData({}, true, Role::ResetEnd);

				m_self.UpdateViewMode();
			};
		} }, 1);
		return model;
	}

private:
	NavigationModelController & m_self;
	Util::Executor & m_executor;
	DB::Database & m_db;
	const NavigationSource m_navigationSource;

	NavigationListItems m_authors;
	NavigationListItems m_series;
	NavigationTreeItems m_genres;
};

NavigationModelController::NavigationModelController(Util::Executor & executor
	, DB::Database & db
	, const NavigationSource navigationSource
	, Settings & uiSettings
)
	: ModelController(uiSettings, GetTypeName(Type::Navigation), Constant::UiSettings_ns::viewModeNavigation, Constant::UiSettings_ns::viewModeNavigation_default, Constant::UiSettings_ns::viewModeValueNavigation)
	, m_impl(*this, executor, db, navigationSource)
{
}

NavigationModelController::~NavigationModelController()
{
	if (auto * model = GetCurrentModel())
		(void)model->setData({}, QVariant::fromValue(To<NavigationModelObserver>()), Role::ObserverUnregister);
}

ModelController::Type NavigationModelController::GetType() const noexcept
{
	return Type::Navigation;
}

QAbstractItemModel * NavigationModelController::CreateModel()
{
	if (auto * model = GetCurrentModel())
		(void)model->setData({}, QVariant::fromValue(To<NavigationModelObserver>()), Role::ObserverUnregister);

	auto * model = m_impl->CreateModel();
	(void)model->setData({}, QVariant::fromValue(To<NavigationModelObserver>()), Role::ObserverRegister);

	return model;
}

}
