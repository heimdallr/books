#include "TreeViewControllerNavigation.h"

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/logic/IBookSearchController.h"
#include "interface/logic/IDatabaseController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/ui/IUiFactory.h"

#include "ChangeNavigationController/GroupController.h"
#include "data/DataItem.h"
#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "model/IModelObserver.h"
#include "shared/BooksContextMenuProvider.h"
#include "util/FunctorExecutionForwarder.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "Navigation";
constexpr auto REMOVE = QT_TRANSLATE_NOOP("Navigation", "Remove");

using ModelCreator = std::shared_ptr<QAbstractItemModel> (IModelProvider::*)(IDataItem::Ptr, IModelObserver&) const;
using MenuRequester = IDataItem::Ptr (*)(ITreeViewController::RequestContextMenuOptions options);

IDataItem::Ptr MenuRequesterStub(ITreeViewController::RequestContextMenuOptions)
{
	return {};
}

#define MENU_ACTION_ITEMS_X_MACRO     \
	MENU_ACTION_ITEM(CreateNewGroup)  \
	MENU_ACTION_ITEM(RemoveGroup)     \
	MENU_ACTION_ITEM(CreateNewSearch) \
	MENU_ACTION_ITEM(RemoveSearch)

enum class MenuAction
{
	Unknown = BooksMenuAction::Last,
#define MENU_ACTION_ITEM(NAME) NAME,
	MENU_ACTION_ITEMS_X_MACRO
#undef MENU_ACTION_ITEM
		Last
};

#define SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEMS_XMACRO            \
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(Groups_User, Groups)   \
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(Searches_User, Search) \
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(Authors, Authors)      \
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(Series, Series)        \
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(Keywords, Keywords)

#define SUBSCRIBED_TABLES_ITEMS_XMACRO       \
	SUBSCRIBED_TABLES_ITEM(Groups_List_User) \
	SUBSCRIBED_TABLES_ITEM(Groups_User)      \
	SUBSCRIBED_TABLES_ITEM(Searches_User)    \
	SUBSCRIBED_TABLES_ITEM(Authors)          \
	SUBSCRIBED_TABLES_ITEM(Series)           \
	SUBSCRIBED_TABLES_ITEM(Keywords)         \
	SUBSCRIBED_TABLES_ITEM(Books)

class ITableSubscriptionHandler // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Function = void (ITableSubscriptionHandler::*)();

public:
	virtual ~ITableSubscriptionHandler() = default;
#define SUBSCRIBED_TABLES_ITEM(NAME) virtual void On_##NAME##_Changed() = 0;
	SUBSCRIBED_TABLES_ITEMS_XMACRO
#undef SUBSCRIBED_TABLES_ITEM
};

constexpr std::pair<std::string_view, ITableSubscriptionHandler::Function> SUBSCRIBED_TABLES[] {
#define SUBSCRIBED_TABLES_ITEM(NAME) { #NAME, &ITableSubscriptionHandler::On_##NAME##_Changed },
	SUBSCRIBED_TABLES_ITEMS_XMACRO
#undef SUBSCRIBED_TABLES_ITEM
};

auto GetSubscribedTable(const std::string_view name)
{
	return FindSecond(SUBSCRIBED_TABLES, name, nullptr);
}

IDataItem::Ptr& Add(const IDataItem::Ptr& dst, QString title, const MenuAction id)
{
	auto item = MenuItem::Create();
	item->SetData(std::move(title), MenuItem::Column::Title);
	item->SetData(QString::number(static_cast<int>(id)), MenuItem::Column::Id);
	return dst->AppendChild(std::move(item));
}

IDataItem::Ptr MenuRequesterGenres(const ITreeViewController::RequestContextMenuOptions options)
{
	if (!(options & ITreeViewController::RequestContextMenuOptions::IsTree))
		return {};

	auto item = MenuItem::Create();
	BooksContextMenuProvider::AddTreeMenuItems(item, options);
	return item;
}

IDataItem::Ptr MenuRequesterGroups(const ITreeViewController::RequestContextMenuOptions options)
{
	auto result = MenuItem::Create();
	Add(result, QT_TRANSLATE_NOOP("Navigation", "Create new group..."), MenuAction::CreateNewGroup);
	auto& removeItem = Add(result, REMOVE, MenuAction::RemoveGroup);
	if (!(options & ITreeViewController::RequestContextMenuOptions::HasSelection))
		removeItem->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);

	return result;
}

IDataItem::Ptr MenuRequesterSearches(const ITreeViewController::RequestContextMenuOptions options)
{
	auto result = MenuItem::Create();
	Add(result, QT_TRANSLATE_NOOP("Navigation", "Create new search..."), MenuAction::CreateNewSearch);
	auto& removeItem = Add(result, REMOVE, MenuAction::RemoveSearch);
	if (!(options & ITreeViewController::RequestContextMenuOptions::HasSelection))
		removeItem->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);

	return result;
}

class IContextMenuHandler // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IContextMenuHandler() = default;
	virtual void OnContextMenuTriggeredStub(const QList<QModelIndex>& indexList, const QModelIndex& index) const = 0;
#define MENU_ACTION_ITEM(NAME) virtual void On##NAME##Triggered(const QList<QModelIndex>& indexList, const QModelIndex& index) const = 0;
	MENU_ACTION_ITEMS_X_MACRO
#undef MENU_ACTION_ITEM
};

using ContextMenuHandlerFunction = void (IContextMenuHandler::*)(const QList<QModelIndex>& indexList, const QModelIndex& index) const;

constexpr std::pair<MenuAction, ContextMenuHandlerFunction> MENU_HANDLERS[] {
#define MENU_ACTION_ITEM(NAME) { MenuAction::NAME, &IContextMenuHandler::On##NAME##Triggered },
	MENU_ACTION_ITEMS_X_MACRO
#undef MENU_ACTION_ITEM
};
static_assert(std::size(MENU_HANDLERS) == static_cast<size_t>(MenuAction::Last) - static_cast<size_t>(MenuAction::Unknown) - 1);

struct ModeDescriptor
{ //-V802
	ViewMode viewMode { ViewMode::Unknown };
	ModelCreator modelCreator { nullptr };
	NavigationMode navigationMode { NavigationMode::Unknown };
	MenuRequester menuRequester { &MenuRequesterStub };
	ContextMenuHandlerFunction createNewAction { nullptr };
	ContextMenuHandlerFunction createRemoveAction { nullptr };
};

constexpr std::pair<const char*, ModeDescriptor> MODE_DESCRIPTORS[] {
	{  Loc::Authors,																				 { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Authors }                    },
	{   Loc::Series,																									  { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Series } },
	{   Loc::Genres,																				{ ViewMode::Tree, &IModelProvider::CreateTreeModel, NavigationMode::Genres, &MenuRequesterGenres } },
	{ Loc::Keywords,																									{ ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Keywords } },
	{ Loc::Archives,																									{ ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Archives } },
	{   Loc::Groups,
     { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::Groups, &MenuRequesterGroups, &IContextMenuHandler::OnCreateNewGroupTriggered, &IContextMenuHandler::OnRemoveGroupTriggered } },
	{   Loc::Search,
     { ViewMode::List,
     &IModelProvider::CreateSearchListModel,
     NavigationMode::Search,
     &MenuRequesterSearches,
     &IContextMenuHandler::OnCreateNewSearchTriggered,
     &IContextMenuHandler::OnRemoveSearchTriggered }																																				  },
};

static_assert(std::size(MODE_DESCRIPTORS) == static_cast<size_t>(NavigationMode::Last));

} // namespace

struct TreeViewControllerNavigation::Impl final
	: IModelObserver
	, virtual IContextMenuHandler
	, virtual private IDatabaseController::IObserver
	, virtual private DB::IDatabaseObserver
	, virtual private ITableSubscriptionHandler
{
	TreeViewControllerNavigation& self;
	std::vector<PropagateConstPtr<QAbstractItemModel, std::shared_ptr>> models;
	std::weak_ptr<const ILogicFactory> logicFactory;
	PropagateConstPtr<IUiFactory, std::shared_ptr> uiFactory;
	PropagateConstPtr<IDatabaseController, std::shared_ptr> databaseController;
	Util::FunctorExecutionForwarder forwarder;
	int mode = { -1 };

	Impl(TreeViewControllerNavigation& self, const std::shared_ptr<const ILogicFactory>& logicFactory, std::shared_ptr<IUiFactory> uiFactory, std::shared_ptr<IDatabaseController> databaseController)
		: self(self)
		, logicFactory(logicFactory)
		, uiFactory(std::move(uiFactory))
		, databaseController(std::move(databaseController))
	{
		for ([[maybe_unused]] const auto& _ : MODE_DESCRIPTORS)
			models.emplace_back(std::shared_ptr<QAbstractItemModel>());

		this->databaseController->RegisterObserver(this);
	}

	~Impl() override
	{
		this->databaseController->UnregisterObserver(this);
	}

private: // IContextMenuHandler
	template <typename T>
	using ControllerCreator = std::shared_ptr<T> (ILogicFactory::*)() const;

	template <typename T>
	void OnCreateNavigationItem(ControllerCreator<T> creator) const
	{
		auto controller = ((*ILogicFactory::Lock(logicFactory)).*creator)();
		controller->CreateNew([=](long long) mutable { controller.reset(); });
	}

	template <typename T>
	void OnRemoveNavigationItem(const QList<QModelIndex>& indexList, const QModelIndex& index, ControllerCreator<T> creator) const
	{
		const auto toId = [](const QModelIndex& ind) { return ind.data(Role::Id).toLongLong(); };

		typename T::Ids ids;
		if (index.isValid())
			ids.emplace(toId(index));
		std::ranges::transform(indexList, std::inserter(ids, ids.end()), toId);
		if (ids.empty())
			return;

		auto controller = ((*ILogicFactory::Lock(logicFactory)).*creator)();
		controller->Remove(std::move(ids), [=](long long) mutable { controller.reset(); });
	}

	void OnContextMenuTriggeredStub(const QList<QModelIndex>&, const QModelIndex&) const override
	{
	}

	void OnCreateNewGroupTriggered(const QList<QModelIndex>&, const QModelIndex&) const override
	{
		OnCreateNavigationItem(&ILogicFactory::CreateGroupController);
	}

	void OnRemoveGroupTriggered(const QList<QModelIndex>& indexList, const QModelIndex& index) const override
	{
		OnRemoveNavigationItem(indexList, index, &ILogicFactory::CreateGroupController);
	}

	void OnCreateNewSearchTriggered(const QList<QModelIndex>&, const QModelIndex&) const override
	{
		OnCreateNavigationItem(&ILogicFactory::CreateSearchController);
	}

	void OnRemoveSearchTriggered(const QList<QModelIndex>& indexList, const QModelIndex& index) const override
	{
		OnRemoveNavigationItem(indexList, index, &ILogicFactory::CreateSearchController);
	}

private: // DatabaseController::IObserver
	void AfterDatabaseCreated(DB::IDatabase& db) override
	{
		db.RegisterObserver(this);
	}

	void BeforeDatabaseDestroyed(DB::IDatabase& db) override
	{
		db.UnregisterObserver(this);
	}

private: // DB::IDatabaseObserver
	void OnInsert(std::string_view /*dbName*/, const std::string_view tableName, int64_t /*rowId*/) override
	{
		if (auto invoker = GetSubscribedTable(tableName))
			forwarder.Forward([this, invoker] { std::invoke(invoker, static_cast<ITableSubscriptionHandler*>(this)); });
	}

	void OnUpdate(std::string_view /*dbName*/, std::string_view /*tableName*/, int64_t /*rowId*/) override
	{
	}

	void OnDelete(const std::string_view dbName, const std::string_view tableName, const int64_t rowId) override
	{
		OnInsert(dbName, tableName, rowId);
	}

private: // ITableSubscriptionHandler
	void On_Groups_List_User_Changed() override
	{
		if (static_cast<NavigationMode>(mode) == NavigationMode::Groups)
			self.RequestBooks(true);
	}

	void On_Books_Changed() override
	{
		OnTableChanged(NavigationMode::Genres);
		OnTableChanged(NavigationMode::Archives);
		self.RequestBooks(true);
	}

#define SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(NAME, TYPE) \
	void On_##NAME##_Changed() override                      \
	{                                                        \
		OnTableChanged(NavigationMode::TYPE);                \
	}
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEMS_XMACRO
#undef SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM

private:
	void OnTableChanged(const NavigationMode tableMode)
	{
		static_cast<NavigationMode>(mode) == tableMode ? self.RequestNavigation(true) : models[static_cast<int>(tableMode)].reset();
	}

	NON_COPY_MOVABLE(Impl)
};

TreeViewControllerNavigation::TreeViewControllerNavigation(std::shared_ptr<ISettings> settings,
                                                           std::shared_ptr<DataProvider> dataProvider,
                                                           const std::shared_ptr<const IModelProvider>& modelProvider,
                                                           const std::shared_ptr<const ILogicFactory>& logicFactory,
                                                           std::shared_ptr<IUiFactory> uiFactory,
                                                           std::shared_ptr<IDatabaseController> databaseController)
	: AbstractTreeViewController(CONTEXT, std::move(settings), std::move(dataProvider), modelProvider)
	, m_impl(*this, logicFactory, std::move(uiFactory), std::move(databaseController))
{
	Setup();

	m_dataProvider->SetNavigationRequestCallback(
		[&](IDataItem::Ptr data)
		{
			const auto modelCreator = MODE_DESCRIPTORS[m_impl->mode].second.modelCreator;
			auto model = std::invoke(modelCreator, IModelProvider::Lock(m_modelProvider), std::move(data), std::ref(*m_impl));
			m_impl->models[m_impl->mode].reset(std::move(model));
			Perform(&IObserver::OnModelChanged, m_impl->models[m_impl->mode].get());
		});

	PLOGV << "TreeViewControllerNavigation created";
}

TreeViewControllerNavigation::~TreeViewControllerNavigation()
{
	PLOGV << "TreeViewControllerNavigation destroyed";
}

void TreeViewControllerNavigation::RequestNavigation(const bool force) const
{
	m_dataProvider->RequestNavigation(force);
}

void TreeViewControllerNavigation::RequestBooks(const bool force) const
{
	m_dataProvider->RequestBooks(force);
}

std::vector<const char*> TreeViewControllerNavigation::GetModeNames() const
{
	return GetModeNamesImpl(MODE_DESCRIPTORS);
}

void TreeViewControllerNavigation::SetCurrentId(ItemType, QString id)
{
	m_dataProvider->SetNavigationId(std::move(id));
}

void TreeViewControllerNavigation::OnModeChanged(const QString& mode)
{
	m_impl->mode = GetModeIndex(mode);
	m_dataProvider->SetNavigationMode(static_cast<NavigationMode>(m_impl->mode));
	Perform(&IObserver::OnModeChanged, m_impl->mode);

	if (m_impl->models[m_impl->mode])
		return Perform(&IObserver::OnModelChanged, m_impl->models[m_impl->mode].get());

	m_dataProvider->RequestNavigation();
}

int TreeViewControllerNavigation::GetModeIndex(const QString& mode) const
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

void TreeViewControllerNavigation::RequestContextMenu(const QModelIndex& index, const RequestContextMenuOptions options, const RequestContextMenuCallback callback)
{
	if (const auto item = MODE_DESCRIPTORS[m_impl->mode].second.menuRequester(options))
		callback(index.data(Role::Id).toString(), item);
}

void TreeViewControllerNavigation::OnContextMenuTriggered(QAbstractItemModel*, const QModelIndex& index, const QList<QModelIndex>& indexList, const IDataItem::Ptr item)
{
	const auto invoker = FindSecond(MENU_HANDLERS, static_cast<MenuAction>(item->GetData(MenuItem::Column::Id).toInt()), &IContextMenuHandler::OnContextMenuTriggeredStub);
	std::invoke(invoker, *m_impl, std::cref(indexList), std::cref(index));
	Perform(&IObserver::OnContextMenuTriggered, index.data(Role::Id).toString(), std::cref(item));
}

ITreeViewController::CreateNewItem TreeViewControllerNavigation::GetNewItemCreator() const
{
	if (const auto creator = MODE_DESCRIPTORS[m_impl->mode].second.createNewAction)
		return [creator, &impl = *m_impl] { std::invoke(creator, impl, QList<QModelIndex> {}, QModelIndex {}); };

	return {};
}

ITreeViewController::RemoveItems TreeViewControllerNavigation::GetRemoveItems() const
{
	if (const auto creator = MODE_DESCRIPTORS[m_impl->mode].second.createRemoveAction)
		return [creator, &impl = *m_impl](const QList<QModelIndex>& list)
		{
			assert(!list.empty());
			std::invoke(creator, impl, std::cref(list), list.front());
		};

	return {};
}
