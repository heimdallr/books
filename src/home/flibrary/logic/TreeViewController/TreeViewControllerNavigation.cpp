#include "TreeViewControllerNavigation.h"

#include <QString>

#include <plog/Log.h>

#include "database/interface/IDatabase.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"

#include "fnd/FindPair.h"

#include "ChangeNavigationController/GroupController.h"
#include "ChangeNavigationController/SearchController.h"
#include "data/DataItem.h"
#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "database/DatabaseController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/ui/IUiFactory.h"
#include "model/IModelObserver.h"
#include "shared/BooksContextMenuProvider.h"
#include "util/FunctorExecutionForwarder.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "Navigation";
constexpr auto REMOVE = QT_TRANSLATE_NOOP("Navigation", "Remove");

using ModelCreator = std::shared_ptr<QAbstractItemModel> (IModelProvider::*)(IDataItem::Ptr, IModelObserver &) const;
using MenuRequester = IDataItem::Ptr(*)(ITreeViewController::RequestContextMenuOptions options);

IDataItem::Ptr MenuRequesterStub(ITreeViewController::RequestContextMenuOptions)
{
	return {};
}

#define MENU_ACTION_ITEMS_X_MACRO         \
		MENU_ACTION_ITEM(CreateNewGroup)  \
		MENU_ACTION_ITEM(RemoveGroup)     \
		MENU_ACTION_ITEM(CreateNewSearch) \
		MENU_ACTION_ITEM(RemoveSearch)

enum class MenuAction
{
		Unknown = BooksMenuAction::Last,
#define MENU_ACTION_ITEM(NAME) NAME,
		MENU_ACTION_ITEMS_X_MACRO
#undef	MENU_ACTION_ITEM
		Last
};

class ITableSubscriptionHandler  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Function = void(ITableSubscriptionHandler::*)();
public:
	virtual ~ITableSubscriptionHandler() = default;
	virtual void On_Groups_User_Changed() = 0;
	virtual void On_Groups_List_User_Changed() = 0;
	virtual void On_Searches_User_Changed() = 0;
};

constexpr std::pair<std::string_view, ITableSubscriptionHandler::Function> SUBSCRIBED_TABLES[]
{
#define ITEM(NAME) {#NAME, &ITableSubscriptionHandler::On_##NAME##_Changed}
		ITEM(Groups_User),
		ITEM(Groups_List_User),
		ITEM(Searches_User),
#undef	ITEM
};

auto GetSubscribedTable(const std::string_view name)
{
	return FindSecond(SUBSCRIBED_TABLES, name, nullptr);
}

void Add(const IDataItem::Ptr & dst, QString title, const MenuAction id)
{
	auto item = MenuItem::Create();
	item->SetData(std::move(title), MenuItem::Column::Title);
	item->SetData(QString::number(static_cast<int>(id)), MenuItem::Column::Id);
	dst->AppendChild(std::move(item));
}

IDataItem::Ptr MenuRequesterGenres(const ITreeViewController::RequestContextMenuOptions options)
{
	if (!(options & ITreeViewController::RequestContextMenuOptions::IsTree))
		return {};

	auto item = MenuItem::Create();
	BooksContextMenuProvider::AddTreeMenuItems(item, options);
	return item;
}

IDataItem::Ptr MenuRequesterGroups(ITreeViewController::RequestContextMenuOptions)
{
	auto result = MenuItem::Create();
	Add(result, QT_TRANSLATE_NOOP("Navigation", "Create new group..."), MenuAction::CreateNewGroup);
	Add(result, REMOVE, MenuAction::RemoveGroup);
	return result;
}

IDataItem::Ptr MenuRequesterSearches(ITreeViewController::RequestContextMenuOptions)
{
	auto result = MenuItem::Create();
	Add(result, QT_TRANSLATE_NOOP("Navigation", "Create new search..."), MenuAction::CreateNewSearch);
	Add(result, REMOVE, MenuAction::RemoveSearch);
	return result;
}

struct ModeDescriptor
{
	ViewMode viewMode { ViewMode::Unknown };
	ModelCreator modelCreator { nullptr };
	NavigationMode navigationMode { NavigationMode::Unknown };
	MenuRequester menuRequester { &MenuRequesterStub };
};

constexpr std::pair<const char *, ModeDescriptor> MODE_DESCRIPTORS[]
{
	{ QT_TRANSLATE_NOOP("Navigation", "Authors") , { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Authors } },
	{ QT_TRANSLATE_NOOP("Navigation", "Series")  , { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Series } },
	{ QT_TRANSLATE_NOOP("Navigation", "Genres")  , { ViewMode::Tree, &IModelProvider::CreateTreeModel, NavigationMode::Genres, &MenuRequesterGenres } },
	{ QT_TRANSLATE_NOOP("Navigation", "Archives"), { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Archives } },
	{ QT_TRANSLATE_NOOP("Navigation", "Groups")  , { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Groups, &MenuRequesterGroups } },
	{ QT_TRANSLATE_NOOP("Navigation", "Search")  , { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Search, &MenuRequesterSearches } },
};

static_assert(std::size(MODE_DESCRIPTORS) == static_cast<size_t>(NavigationMode::Last));

class IContextMenuHandler  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IContextMenuHandler() = default;
	virtual void OnContextMenuTriggeredStub(const QList<QModelIndex> & indexList, const QModelIndex & index) const = 0;
#define MENU_ACTION_ITEM(NAME) virtual void On##NAME##Triggered(const QList<QModelIndex> & indexList, const QModelIndex & index) const = 0;
		MENU_ACTION_ITEMS_X_MACRO
#undef	MENU_ACTION_ITEM
};

using ContextMenuHandlerFunction = void (IContextMenuHandler::*)(const QList<QModelIndex> & indexList, const QModelIndex & index) const;

constexpr std::pair<MenuAction, ContextMenuHandlerFunction> MENU_HANDLERS[]
{
#define MENU_ACTION_ITEM(NAME) { MenuAction::NAME, &IContextMenuHandler::On##NAME##Triggered },
		MENU_ACTION_ITEMS_X_MACRO
#undef	MENU_ACTION_ITEM
};

}

struct TreeViewControllerNavigation::Impl final
	: IModelObserver
	, virtual IContextMenuHandler
	, virtual private DatabaseController::IObserver
	, virtual private DB::IDatabaseObserver
	, virtual private ITableSubscriptionHandler
{
	TreeViewControllerNavigation & self;
	std::vector<PropagateConstPtr<QAbstractItemModel, std::shared_ptr>> models;
	PropagateConstPtr<ILogicFactory, std::shared_ptr> logicFactory;
	PropagateConstPtr<IUiFactory, std::shared_ptr> uiFactory;
	PropagateConstPtr<DatabaseController, std::shared_ptr> databaseController;
	Util::FunctorExecutionForwarder forwarder;
	int mode = { -1 };

	Impl(TreeViewControllerNavigation & self
		, std::shared_ptr<ILogicFactory> logicFactory
		, std::shared_ptr<IUiFactory> uiFactory
		, std::shared_ptr<DatabaseController> databaseController
	)
		: self(self)
		, logicFactory(std::move(logicFactory))
		, uiFactory(std::move(uiFactory))
		, databaseController(std::move(databaseController))
	{
		for ([[maybe_unused]]const auto & _ : MODE_DESCRIPTORS)
			models.emplace_back(std::shared_ptr<QAbstractItemModel>());

		this->databaseController->RegisterObserver(this);
	}

	~Impl() override
	{
		this->databaseController->UnregisterObserver(this);
	}

private: // IContextMenuHandler
	template <typename T>
	using ControllerCreator = std::shared_ptr<T>(ILogicFactory::*)() const;
	template <typename T>
	void OnCreateNavigationItem(ControllerCreator<T> creator) const
	{
		auto controller = ((*logicFactory).*creator)();
		controller->CreateNew([=] () mutable
		{
			controller.reset();
		});
	}

	template <typename T>
	void OnRemoveNavigationItem(const QList<QModelIndex> & indexList, const QModelIndex & index, ControllerCreator<T> creator) const
	{
		const auto toId = [] (const QModelIndex & ind)
		{
			return ind.data(Role::Id).toLongLong();
		};

		typename T::Ids ids { toId(index) };
		std::ranges::transform(indexList, std::inserter(ids, ids.end()), toId);
		if (ids.empty())
			return;

		auto controller = ((*logicFactory).*creator)();
		controller->Remove(std::move(ids), [=] () mutable
		{
			controller.reset();
		});
	}

	void OnContextMenuTriggeredStub(const QList<QModelIndex> &, const QModelIndex &) const override
	{
	}

	void OnCreateNewGroupTriggered(const QList<QModelIndex> &, const QModelIndex &) const override
	{
		OnCreateNavigationItem(&ILogicFactory::CreateGroupController);
	}

	void OnRemoveGroupTriggered(const QList<QModelIndex> & indexList, const QModelIndex & index) const override
	{
		OnRemoveNavigationItem(indexList, index, &ILogicFactory::CreateGroupController);
	}

	void OnCreateNewSearchTriggered(const QList<QModelIndex> &, const QModelIndex &) const override
	{
		OnCreateNavigationItem(&ILogicFactory::CreateSearchController);
	}

	void OnRemoveSearchTriggered(const QList<QModelIndex> & indexList, const QModelIndex & index) const override
	{
		OnRemoveNavigationItem(indexList, index, &ILogicFactory::CreateSearchController);
	}

private: // DatabaseController::IObserver
	void AfterDatabaseCreated(DB::IDatabase & db) override
	{
		db.RegisterObserver(this);
	}

	void BeforeDatabaseDestroyed(DB::IDatabase & db) override
	{
		db.UnregisterObserver(this);
	}

private: // DB::IDatabaseObserver
	void OnInsert(std::string_view /*dbName*/, const std::string_view tableName, int64_t /*rowId*/) override
	{
		if (auto invoker = GetSubscribedTable(tableName))
			forwarder.Forward([this, invoker]
		{
			((*this).*invoker)();
		});
	}
	void OnUpdate(std::string_view /*dbName*/, std::string_view /*tableName*/, int64_t /*rowId*/) override
	{
	}
	void OnDelete(std::string_view /*dbName*/, const std::string_view tableName, int64_t /*rowId*/) override
	{
		if (auto invoker = GetSubscribedTable(tableName))
			forwarder.Forward([this, invoker]
		{
			((*this).*invoker)();
		});
	}

private: // ITableSubscriptionHandler
	void On_Groups_User_Changed() override
	{
		if (static_cast<NavigationMode>(mode) == NavigationMode::Groups)
			self.RequestNavigation();
		else
			models[static_cast<int>(NavigationMode::Groups)].reset();
	}

	void On_Groups_List_User_Changed() override
	{
		if (static_cast<NavigationMode>(mode) == NavigationMode::Groups)
			self.RequestBooks(true);
	}

	void On_Searches_User_Changed() override
	{
		if (static_cast<NavigationMode>(mode) == NavigationMode::Search)
			self.RequestNavigation();
		else
			models[static_cast<int>(NavigationMode::Search)].reset();
	}

	NON_COPY_MOVABLE(Impl)
};

TreeViewControllerNavigation::TreeViewControllerNavigation(std::shared_ptr<ISettings> settings
	, std::shared_ptr<DataProvider> dataProvider
	, std::shared_ptr<IModelProvider> modelProvider
	, std::shared_ptr<ILogicFactory> logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
	, std::shared_ptr<DatabaseController> databaseController
)
	: AbstractTreeViewController(CONTEXT
		, std::move(settings)
		, std::move(dataProvider)
		, std::move(modelProvider)
	)
	, m_impl(*this
		, std::move(logicFactory)
		, std::move(uiFactory)
		, std::move(databaseController)
	)
{
	Setup();

	m_dataProvider->SetNavigationRequestCallback([&] (IDataItem::Ptr data)
	{
		const auto modelCreator = MODE_DESCRIPTORS[m_impl->mode].second.modelCreator;
		auto model = std::invoke(modelCreator, m_modelProvider, std::move(data), std::ref(*m_impl));
		m_impl->models[m_impl->mode].reset(std::move(model));
		Perform(&IObserver::OnModelChanged, m_impl->models[m_impl->mode].get());
	});

	PLOGD << "TreeViewControllerNavigation created";
}

TreeViewControllerNavigation::~TreeViewControllerNavigation()
{
	PLOGD << "TreeViewControllerNavigation destroyed";
}

void TreeViewControllerNavigation::RequestNavigation() const
{
	m_dataProvider->RequestNavigation();
}

void TreeViewControllerNavigation::RequestBooks(const bool force) const
{
	m_dataProvider->RequestBooks(force);
}

std::vector<const char *> TreeViewControllerNavigation::GetModeNames() const
{
	return GetModeNamesImpl(MODE_DESCRIPTORS);
}

void TreeViewControllerNavigation::SetCurrentId(QString id)
{
	m_dataProvider->SetNavigationId(std::move(id));
}

void TreeViewControllerNavigation::OnModeChanged(const QString & mode)
{
	m_impl->mode = GetModeIndex(mode);
	m_dataProvider->SetNavigationMode(static_cast<NavigationMode>(m_impl->mode));
	Perform(&IObserver::OnModeChanged, m_impl->mode);

	if (m_impl->models[m_impl->mode])
		return Perform(&IObserver::OnModelChanged, m_impl->models[m_impl->mode].get());

	m_dataProvider->RequestNavigation();
}

int TreeViewControllerNavigation::GetModeIndex(const QString & mode) const
{
	const auto strMode = mode.toStdString();
	const auto enumMode = FindSecond(MODE_DESCRIPTORS, strMode.data(), MODE_DESCRIPTORS[0].second, PszComparerCaseInsensitive {}).navigationMode;
	return static_cast<int>(enumMode);
}

ItemType TreeViewControllerNavigation::GetItemType() const noexcept
{
	return ItemType::Navigation;
}

ViewMode TreeViewControllerNavigation::GetViewMode() const noexcept
{
	return MODE_DESCRIPTORS[m_impl->mode].second.viewMode;
}

void TreeViewControllerNavigation::RequestContextMenu(const QModelIndex & index, const RequestContextMenuOptions options, RequestContextMenuCallback callback)
{
	if (const auto item = MODE_DESCRIPTORS[m_impl->mode].second.menuRequester(options))
		callback(index.data(Role::Id).toString(), item);
}

void TreeViewControllerNavigation::OnContextMenuTriggered(QAbstractItemModel *, const QModelIndex & index, const QList<QModelIndex> & indexList, const IDataItem::Ptr item)
{
	const auto invoker = FindSecond(MENU_HANDLERS, static_cast<MenuAction>(item->GetData(MenuItem::Column::Id).toInt()), &IContextMenuHandler::OnContextMenuTriggeredStub);
	std::invoke(invoker, *m_impl, std::cref(indexList), std::cref(index));
	Perform(&IObserver::OnContextMenuTriggered, index.data(Role::Id).toString(), std::cref(item));
}
