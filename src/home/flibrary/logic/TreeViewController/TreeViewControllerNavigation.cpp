#include "TreeViewControllerNavigation.h"

#include <qglobal.h>
#include <QModelIndex>
#include <QVariant>

#include <plog/Log.h>

#include "database/interface/IDatabase.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"

#include "fnd/FindPair.h"

#include "data/DataItem.h"
#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/ui/IUiFactory.h"
#include "model/IModelObserver.h"
#include "shared/DatabaseController.h"
#include "shared/GroupController.h"
#include "util/FunctorExecutionForwarder.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "Navigation";

using ModelCreator = std::shared_ptr<QAbstractItemModel> (IModelProvider::*)(IDataItem::Ptr, IModelObserver &) const;
using MenuRequester = IDataItem::Ptr(*)();

IDataItem::Ptr MenuRequesterStub()
{
	return nullptr;
}

enum class MenuAction
{
	CreateNewGroup,
	RemoveGroup,
};

class ITableSubscriptionHandler
{
public:
	using Function = void(ITableSubscriptionHandler::*)();
public:
	virtual ~ITableSubscriptionHandler() = default;
	virtual void On_Groups_User_Changed() = 0;
	virtual void On_Groups_List_User_Changed() = 0;
};

constexpr std::pair<std::string_view, ITableSubscriptionHandler::Function> SUBSCRIBED_TABLES[]
{
#define ITEM(NAME) {#NAME, &ITableSubscriptionHandler::On_##NAME##_Changed}
		ITEM(Groups_User),
		ITEM(Groups_List_User),
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

IDataItem::Ptr MenuRequesterGroups()
{
	auto result = MenuItem::Create();
	Add(result, QT_TRANSLATE_NOOP("Navigation", "Create new..."), MenuAction::CreateNewGroup);
	Add(result, QT_TRANSLATE_NOOP("Navigation", "Remove"), MenuAction::RemoveGroup);
	return result;
}

struct ModeDescriptor
{
	ViewMode viewMode;
	ModelCreator modelCreator;
	NavigationMode navigationMode;
	MenuRequester menuRequester { &MenuRequesterStub };
};

constexpr std::pair<const char *, ModeDescriptor> MODE_DESCRIPTORS[]
{
	{ QT_TRANSLATE_NOOP("Navigation", "Authors") , { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Authors } },
	{ QT_TRANSLATE_NOOP("Navigation", "Series")  , { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Series } },
	{ QT_TRANSLATE_NOOP("Navigation", "Genres")  , { ViewMode::Tree, &IModelProvider::CreateTreeModel, NavigationMode::Genres } },
	{ QT_TRANSLATE_NOOP("Navigation", "Archives"), { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Archives } },
	{ QT_TRANSLATE_NOOP("Navigation", "Groups")  , { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Groups, &MenuRequesterGroups } },
};

static_assert(std::size(MODE_DESCRIPTORS) == static_cast<size_t>(NavigationMode::Last));
static_assert(std::ranges::all_of(MODE_DESCRIPTORS, [] (const auto & )
{
	return true;
}));

class IContextMenuHandler  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IContextMenuHandler() = default;
	virtual void OnCreateNewGroupTriggered(const QList<QModelIndex> & indexList, const QModelIndex & index) const = 0;
	virtual void OnRemoveGroupTriggered(const QList<QModelIndex> & indexList, const QModelIndex & index) const = 0;
};

using ContextMenuHandlerFunction = void (IContextMenuHandler::*)(const QList<QModelIndex> & indexList, const QModelIndex & index) const;

constexpr std::pair<MenuAction, ContextMenuHandlerFunction> MENU_HANDLERS[]
{
	{ MenuAction::CreateNewGroup, &IContextMenuHandler::OnCreateNewGroupTriggered },
	{ MenuAction::RemoveGroup, &IContextMenuHandler::OnRemoveGroupTriggered },
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
	mutable std::shared_ptr<GroupController> groupController;
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
	void OnCreateNewGroupTriggered(const QList<QModelIndex> &, const QModelIndex &) const override
	{
		if (!groupController)
			groupController = logicFactory->CreateGroupController();

		groupController->CreateNewGroup([&, gc = groupController] () mutable
		{
			gc.reset();
		});
	}

	void OnRemoveGroupTriggered(const QList<QModelIndex> & indexList, const QModelIndex & index) const override
	{
		const auto toId = [] (const QModelIndex & ind)
		{
			return ind.data(Role::Id).toInt();
		};

		GroupController::Ids ids { toId(index) };
		std::ranges::transform(indexList, std::inserter(ids, ids.end()), toId);
		if (ids.empty())
			return;

		if (!groupController)
			groupController = logicFactory->CreateGroupController();

		groupController->RemoveGroups(std::move(ids), [&, gc = groupController]() mutable
		{
			gc.reset();
		});
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

void TreeViewControllerNavigation::RequestContextMenu(const QModelIndex & index)
{
	const auto item = MODE_DESCRIPTORS[m_impl->mode].second.menuRequester();
	Perform(&IObserver::OnContextMenuReady, index.data(Role::Id).toString(), std::cref(item));
}

void TreeViewControllerNavigation::OnContextMenuTriggered(QAbstractItemModel *, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item) const
{
	const auto invoker = FindSecond(MENU_HANDLERS, static_cast<MenuAction>(item->GetData(MenuItem::Column::Id).toInt()));
	std::invoke(invoker, *m_impl, std::cref(indexList), std::cref(index));
}
