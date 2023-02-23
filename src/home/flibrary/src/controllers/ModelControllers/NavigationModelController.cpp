#include <stack>

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QPointer>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "constants/Localization.h"

#include "models/NavigationListItem.h"
#include "models/NavigationTreeItem.h"
#include "models/RoleBase.h"

#include "util/IExecutor.h"

#include "ModelControllerSettings.h"
#include "NavigationModelController.h"

#include "Settings/UiSettings_keys.h"
#include "Settings/UiSettings_values.h"

namespace HomeCompa::Flibrary {

namespace {

using Role = RoleBase;

constexpr auto AUTHORS_QUERY = "select AuthorID, FirstName, LastName, MiddleName from Authors";
constexpr auto SERIES_QUERY = "select SeriesID, SeriesTitle from Series";
constexpr auto GENRES_QUERY = "select GenreCode, ParentCode, GenreAlias from Genres";
constexpr auto GROUPS_QUERY = "select GroupID, Title from Groups_User";
constexpr auto ARCHIVES_QUERY = "select distinct Folder from Books";

void AppendTitle(QString & title, std::string_view str)
{
	if (title.isEmpty())
	{
		title = str.data();
		return;
	}

	if (!str.empty())
		title.append(" ").append(str.data());
}

QString CreateAuthorTitle(const DB::IQuery & query)
{
	QString title = query.Get<const char *>(2);
	AppendTitle(title, query.Get<const char *>(1));
	AppendTitle(title, query.Get<const char *>(3));

	if (title.isEmpty())
		title = QCoreApplication::translate(Constant::Localization::CONTEXT_ERROR, Constant::Localization::AUTHOR_NOT_SPECIFIED);

	return title;
}

NavigationListItems CreateAuthors(DB::IDatabase & db)
{
	NavigationListItems items;
	const auto query = db.CreateQuery(AUTHORS_QUERY);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		items.emplace_back();
		items.back().Id = QString::number(query->Get<int>(0));
		items.back().Title = CreateAuthorTitle(*query);
	}

	std::ranges::sort(items, [] (const NavigationListItem & lhs, const NavigationListItem & rhs)
	{
		return QString::compare(lhs.Title, rhs.Title, Qt::CaseInsensitive) < 0;
	});

	return items;
}

NavigationListItems CreateDictionary(DB::IDatabase & db, std::string_view queryText)
{
	NavigationListItems items;
	const auto query = db.CreateQuery(queryText);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		items.emplace_back();
		items.back().Id = QString::number(query->Get<int>(0));
		items.back().Title = query->Get<const char *>(1);
	}

	std::ranges::sort(items, [] (const NavigationListItem & lhs, const NavigationListItem & rhs)
	{
		return QString::compare(lhs.Title, rhs.Title, Qt::CaseInsensitive) < 0;
	});

	return items;
}

NavigationListItems CreateSeries(DB::IDatabase & db)
{
	return CreateDictionary(db, SERIES_QUERY);
}

NavigationListItems CreateGroups(DB::IDatabase & db)
{
	return CreateDictionary(db, GROUPS_QUERY);
}

NavigationListItems CreateArchives(DB::IDatabase & db)
{
	NavigationListItems items;
	const auto query = db.CreateQuery(ARCHIVES_QUERY);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		items.emplace_back();
		items.back().Id = items.back().Title = query->Get<const char *>(0);
	}

	std::ranges::sort(items, [] (const NavigationListItem & lhs, const NavigationListItem & rhs)
	{
		return QString::compare(lhs.Title, rhs.Title, Qt::CaseInsensitive) < 0;
	});

	return items;
}

namespace GenresDetails {

NavigationTreeItems SelectGenres(DB::IDatabase & db)
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

NavigationTreeItems CreateGenres(DB::IDatabase & db)
{
	NavigationTreeItems items = GenresDetails::SelectGenres(db);
	return GenresDetails::ReorderGenres(items);
}

template<typename T>
using NavigationItemsCreator = std::vector<T>(*)(DB::IDatabase & db);

std::unique_ptr<ModelControllerSettings> CreateModelControllerSettings(Settings & uiSettings)
{
	return std::make_unique<ModelControllerSettings>(uiSettings
#define	MODEL_CONTROLLER_SETTINGS_ITEM(NAME) , HomeCompa::Constant::UiSettings_ns::NAME##Navigation, HomeCompa::Constant::UiSettings_ns::NAME##Navigation_default
		MODEL_CONTROLLER_SETTINGS_ITEMS_XMACRO
#undef  MODEL_CONTROLLER_SETTINGS_ITEM
	);
}

}

QAbstractItemModel * CreateNavigationModel(NavigationListItems & items, QObject * parent = nullptr);
QAbstractItemModel * CreateNavigationModel(NavigationTreeItems & items, QObject * parent = nullptr);

class NavigationModelController::Impl
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(NavigationModelController & self, Util::IExecutor & executor, DB::IDatabase & db, const NavigationSource navigationSource)
		: m_self(self)
		, m_executor(executor)
		, m_db(db)
		, m_navigationSource(navigationSource)
	{
	}

	~Impl()
	{
		m_self.SaveCurrentItemId();
	}

	void UpdateModelData(QAbstractItemModel * model)
	{
		switch (m_navigationSource)
		{
			case NavigationSource::Authors:
				return ReloadModelDataImpl<NavigationListItem>(model, &CreateAuthors, m_authors);
			case NavigationSource::Series:
				return ReloadModelDataImpl<NavigationListItem>(model, &CreateSeries, m_series);
			case NavigationSource::Groups:
				return ReloadModelDataImpl<NavigationListItem>(model, &CreateGroups, m_groups);
			case NavigationSource::Archives:
				return ReloadModelDataImpl<NavigationListItem>(model, &CreateArchives, m_archives);
			case NavigationSource::Genres:
				return ReloadModelDataImpl<NavigationTreeItem>(model, &CreateGenres, m_genres);
			default:
				break;
		}
		throw std::invalid_argument("unexpected model source");
	}

	QAbstractItemModel * CreateModel()
	{
		switch(m_navigationSource)
		{
			case NavigationSource::Authors:
				return CreateModelImpl<NavigationListItem>(&CreateAuthors, m_authors);
			case NavigationSource::Series:
				return CreateModelImpl<NavigationListItem>(&CreateSeries, m_series);
			case NavigationSource::Groups:
				return CreateModelImpl<NavigationListItem>(&CreateGroups, m_groups);
			case NavigationSource::Archives:
				return CreateModelImpl<NavigationListItem>(&CreateArchives, m_archives);
			case NavigationSource::Genres:
				return CreateModelImpl<NavigationTreeItem>(&CreateGenres, m_genres);
			default:
				break;
		}
		throw std::invalid_argument("unexpected model source");
	}

private:
	template<typename T>
	void ReloadModelDataImpl(QAbstractItemModel * model, NavigationItemsCreator<T> creator, std::vector<T> & navigationItems) const
	{
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

				m_self.FindCurrentItem();
			};
		} }, 1);
	}

	template<typename T>
	QAbstractItemModel * CreateModelImpl(NavigationItemsCreator<T> creator, std::vector<T> & navigationItems) const
	{
		auto * model = CreateNavigationModel(navigationItems);
		ReloadModelDataImpl<T>(model, creator, navigationItems);
		return model;
	}

private:
	NavigationModelController & m_self;
	Util::IExecutor & m_executor;
	DB::IDatabase & m_db;
	const NavigationSource m_navigationSource;

	NavigationListItems m_authors;
	NavigationListItems m_series;
	NavigationListItems m_groups;
	NavigationTreeItems m_genres;
	NavigationListItems m_archives;
};

NavigationModelController::NavigationModelController(Util::IExecutor & executor
	, DB::IDatabase & db
	, const NavigationSource navigationSource
	, Settings & uiSettings
)
	: ModelController(CreateModelControllerSettings(uiSettings)
		, GetTypeName(ModelControllerType::Navigation)
		, FindSecond(g_viewSourceNavigationModelItems, navigationSource)
	)
	, m_impl(*this, executor, db, navigationSource)
{
}

NavigationModelController::~NavigationModelController()
{
	if (auto * model = GetCurrentModel())
		(void)model->setData({}, QVariant::fromValue(To<NavigationModelObserver>()), Role::ObserverUnregister);
}

ModelControllerType NavigationModelController::GetType() const noexcept
{
	return ModelControllerType::Navigation;
}

void NavigationModelController::UpdateModelData()
{
	auto * const model = GetCurrentModel();
	assert(model);
	m_impl->UpdateModelData(model);
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
