#include "TreeViewControllerNavigation.h"

#include <ranges>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IBookSearchController.h"

#include "ChangeNavigationController/GroupController.h"
#include "data/DataItem.h"
#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "shared/BooksContextMenuProvider.h"
#include "shared/MenuItems.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/SortString.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "Navigation";
constexpr auto REMOVE = QT_TRANSLATE_NOOP("Navigation", "Remove");

using ModelCreator = std::shared_ptr<QAbstractItemModel> (IModelProvider::*)(IDataItem::Ptr, bool) const;
using Callback = std::function<void()>;

TR_DEF

template <typename T>
std::unordered_set<T> GetSelected(const QModelIndex& index, const QList<QModelIndex>& indexList)
{
	std::unordered_set<T> ids { index.data(Role::Id).value<T>() };
	ids.reserve(indexList.size() + 1);
	std::ranges::transform(indexList, std::inserter(ids, ids.end()), [](const auto& item) { return item.data(Role::Id).template value<T>(); });
	return ids;
}

#define SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEMS_X_MACRO           \
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(Authors, Authors)      \
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(Genres, Genres)        \
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(Groups_User, Groups)   \
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(Keywords, Keywords)    \
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(Searches_User, Search) \
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(Series, Series)        \
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(Updates, Updates)

#define SUBSCRIBED_TABLES_ITEMS_X_MACRO      \
	SUBSCRIBED_TABLES_ITEM(Authors)          \
	SUBSCRIBED_TABLES_ITEM(Books)            \
	SUBSCRIBED_TABLES_ITEM(Genres)           \
	SUBSCRIBED_TABLES_ITEM(Groups_List_User) \
	SUBSCRIBED_TABLES_ITEM(Groups_User)      \
	SUBSCRIBED_TABLES_ITEM(Keywords)         \
	SUBSCRIBED_TABLES_ITEM(Searches_User)    \
	SUBSCRIBED_TABLES_ITEM(Series)           \
	SUBSCRIBED_TABLES_ITEM(Updates)

class ITableSubscriptionHandler // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Function = void (ITableSubscriptionHandler::*)();

public:
	virtual ~ITableSubscriptionHandler() = default;
#define SUBSCRIBED_TABLES_ITEM(NAME) virtual void On_##NAME##_Changed() = 0;
	SUBSCRIBED_TABLES_ITEMS_X_MACRO
#undef SUBSCRIBED_TABLES_ITEM
};

constexpr std::pair<std::string_view, ITableSubscriptionHandler::Function> SUBSCRIBED_TABLES[] {
#define SUBSCRIBED_TABLES_ITEM(NAME) { #NAME, &ITableSubscriptionHandler::On_##NAME##_Changed },
	SUBSCRIBED_TABLES_ITEMS_X_MACRO
#undef SUBSCRIBED_TABLES_ITEM
};

auto GetSubscribedTable(const std::string_view name)
{
	return FindSecond(SUBSCRIBED_TABLES, name, nullptr);
}

class IContextMenuHandler // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IContextMenuHandler() = default;
	virtual void OnContextMenuTriggeredStub(const QList<QModelIndex>& indexList, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const = 0;
#define MENU_ACTION_ITEM(NAME) virtual void On##NAME##Triggered(const QList<QModelIndex>& indexList, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const = 0;
	GROUPS_MENU_ACTION_ITEMS_X_MACRO
	NAVIGATION_MENU_ACTION_ITEMS_X_MACRO
#undef MENU_ACTION_ITEM
};

using ContextMenuHandlerFunction = void (IContextMenuHandler::*)(const QList<QModelIndex>& indexList, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const;

constexpr std::pair<int, ContextMenuHandlerFunction> MENU_HANDLERS[] {
#define MENU_ACTION_ITEM(NAME) { GroupsMenuAction::NAME, &IContextMenuHandler::On##NAME##Triggered },
	GROUPS_MENU_ACTION_ITEMS_X_MACRO
#undef MENU_ACTION_ITEM
#define MENU_ACTION_ITEM(NAME) { MenuAction::NAME, &IContextMenuHandler::On##NAME##Triggered },
		NAVIGATION_MENU_ACTION_ITEMS_X_MACRO
#undef MENU_ACTION_ITEM
};

struct ModeDescriptor;

class INavigationFilter // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~INavigationFilter() = default;
	virtual bool Stub(const ModeDescriptor&) const = 0;
	virtual bool IsRecordExists(const ModeDescriptor&) const = 0;
	virtual bool IsFolderExists(const ModeDescriptor&) const = 0;
};

class IContextMenuProvider // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IContextMenuProvider() = default;
#define NAVIGATION_MODE_ITEM(NAME) virtual IDataItem::Ptr Create##NAME##ContextMenu(DB::IDatabase& db, const QString& id, ITreeViewController::RequestContextMenuOptions options) = 0;
	NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM
};

using MenuRequester = IDataItem::Ptr (IContextMenuProvider::*)(DB::IDatabase& db, const QString& id, ITreeViewController::RequestContextMenuOptions options);

constexpr MenuRequester MENU_REQUESTERS[] {
#define NAVIGATION_MODE_ITEM(NAME) &IContextMenuProvider::Create##NAME##ContextMenu,
	NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM
};

struct ModeDescriptor
{ //-V802

	using Filter = bool (INavigationFilter::*)(const ModeDescriptor&) const;

	ViewMode viewMode { ViewMode::Unknown };
	ModelCreator modelCreator { nullptr };
	NavigationMode navigationMode { NavigationMode::Unknown };
	Filter filterInvoker { &INavigationFilter::Stub };
	const char* filterParameter { nullptr };
	ContextMenuHandlerFunction createNewAction { nullptr };
	ContextMenuHandlerFunction createRemoveAction { nullptr };
};

constexpr std::pair<const char*, ModeDescriptor> MODE_DESCRIPTORS[] {
	{	  Loc::Authors,
     {
     ViewMode::List,
     &IModelProvider::CreateAuthorsListModel,
     NavigationMode::Authors,
     &INavigationFilter::IsRecordExists,
     "select exists (select 42 from Authors where AuthorId > (select min(AuthorId) from Authors))",
     }																																			   },
	{	   Loc::Series,
     {
     ViewMode::List,
     &IModelProvider::CreateListModel,
     NavigationMode::Series,
     &INavigationFilter::IsRecordExists,
     "select exists (select 42 from Series where SeriesId > (select min(SeriesId) from Series))",
     }																																			   },
	{	   Loc::Genres,
     {
     ViewMode::Tree,
     &IModelProvider::CreateTreeModel,
     NavigationMode::Genres,
     &INavigationFilter::IsRecordExists,
     "select exists (select 42 from Genres g join Genre_List l on l.GenreCode = g.GenreCode where g.rowid > (select min(g.rowid) from Genres g join Genre_List l on l.GenreCode = g.GenreCode))",
     }																																			   },
	{ Loc::PublishYears,
     {
     ViewMode::List,
     &IModelProvider::CreateListModel,
     NavigationMode::PublishYear,
     &INavigationFilter::IsRecordExists,
     "select exists (select 42 from Books where Year IS NOT NULL)",
     }																																			   },
	{     Loc::Keywords,
     {
     ViewMode::List,
     &IModelProvider::CreateListModel,
     NavigationMode::Keywords,
     &INavigationFilter::IsRecordExists,
     "select exists (select 42 from Keywords where KeywordId > (select min(KeywordId) from Keywords))",
     }																																			   },
	{	  Loc::Updates,
     {
     ViewMode::Tree,
     &IModelProvider::CreateTreeModel,
     NavigationMode::Updates,
     &INavigationFilter::IsRecordExists,
     "select exists (select 42 from Updates u join Books b on b.UpdateID = u.UpdateID where u.UpdateID > (select min(u.UpdateID) from Updates u join Books b on b.UpdateID = u.UpdateID))",
     }																																			   },
	{     Loc::Archives,
     { ViewMode::List,
     &IModelProvider::CreateListModel,
     NavigationMode::Archives,
     &INavigationFilter::IsRecordExists,
     "select exists (select 42 from Folders where FolderId > (select min(FolderId) from Folders))" }                                                 },

	{    Loc::Languages,
     {
     ViewMode::List,
     &IModelProvider::CreateListModel,
     NavigationMode::Languages,
     &INavigationFilter::IsRecordExists,
     "select exists (select 42 from Books where Lang > (select min(Lang) from Books))",
     }																																			   },
	{	   Loc::Groups,
     { ViewMode::List,
     &IModelProvider::CreateListModel,
     NavigationMode::Groups,
     &INavigationFilter::Stub,
     nullptr,
     &IContextMenuHandler::OnCreateNewGroupTriggered,
     &IContextMenuHandler::OnRemoveGroupTriggered }																								  },

	{	   Loc::Search,
     { ViewMode::List,
     &IModelProvider::CreateSearchListModel,
     NavigationMode::Search,
     &INavigationFilter::Stub,
     nullptr,
     &IContextMenuHandler::OnCreateNewSearchTriggered,
     &IContextMenuHandler::OnRemoveSearchTriggered }																								 },

	{	  Loc::Reviews, { ViewMode::Tree, &IModelProvider::CreateTreeModel, NavigationMode::Reviews, &INavigationFilter::IsFolderExists, "reviews" } },
	{     Loc::AllBooks,											   { ViewMode::List, &IModelProvider::CreateListModel, NavigationMode::AllBooks } },
};

static_assert(std::size(MODE_DESCRIPTORS) == static_cast<size_t>(NavigationMode::Last));

} // namespace

struct TreeViewControllerNavigation::Impl final
	: virtual IContextMenuHandler
	, virtual private IDatabaseController::IObserver
	, virtual private DB::IDatabaseObserver
	, virtual private ITableSubscriptionHandler
	, virtual private INavigationFilter
	, virtual private IContextMenuProvider
{
	TreeViewControllerNavigation& self;
	mutable std::vector<PropagateConstPtr<QAbstractItemModel, std::shared_ptr>> models;
	std::weak_ptr<const ILogicFactory> logicFactory;
	std::shared_ptr<const ICollectionProvider> collectionProvider;
	PropagateConstPtr<INavigationInfoProvider, std::shared_ptr> dataProvider;
	PropagateConstPtr<IUiFactory, std::shared_ptr> uiFactory;
	PropagateConstPtr<IDatabaseController, std::shared_ptr> databaseController;
	PropagateConstPtr<IAuthorAnnotationController, std::shared_ptr> authorAnnotationController;
	std::shared_ptr<IFilterController> filterController;
	const std::vector<std::pair<const char*, int>> modes { GetModes() };
	Util::FunctorExecutionForwarder forwarder;
	int mode { -1 };

	Impl(TreeViewControllerNavigation& self,
	     const std::shared_ptr<const ILogicFactory>& logicFactory,
	     std::shared_ptr<const ICollectionProvider> collectionProvider,
	     std::shared_ptr<INavigationInfoProvider> dataProvider,
	     std::shared_ptr<IUiFactory> uiFactory,
	     std::shared_ptr<IDatabaseController> databaseController,
	     std::shared_ptr<IAuthorAnnotationController> authorAnnotationController,
	     std::shared_ptr<IFilterController> filterController)
		: self { self }
		, logicFactory { logicFactory }
		, collectionProvider { std::move(collectionProvider) }
		, dataProvider { std::move(dataProvider) }
		, uiFactory { std::move(uiFactory) }
		, databaseController { std::move(databaseController) }
		, authorAnnotationController { std::move(authorAnnotationController) }
		, filterController { std::move(filterController) }
	{
		for ([[maybe_unused]] const auto& _ : MODE_DESCRIPTORS)
			models.emplace_back(std::shared_ptr<QAbstractItemModel>());

		this->databaseController->RegisterObserver(this);
	}

	~Impl() override
	{
		this->databaseController->UnregisterObserver(this);
	}

	void RequestContextMenu(const QModelIndex& index, RequestContextMenuOptions options, RequestContextMenuCallback callback)
	{
		assert(static_cast<size_t>(mode) < std::size(MENU_REQUESTERS));
		auto id = index.data(Role::Id).toString();
		const auto db = databaseController->GetDatabase(true);
		if (auto item = std::invoke(MENU_REQUESTERS[mode], static_cast<IContextMenuProvider*>(this), std::ref(*db), std::cref(id), options))
			forwarder.Forward([callback = std::move(callback), id = std::move(id), item = std::move(item)] { callback(id, item); });
	}

private: // IContextMenuHandler
	void OnContextMenuTriggeredStub(const QList<QModelIndex>&, const QModelIndex&, const IDataItem::Ptr&, Callback callback) const override
	{
		forwarder.Forward(std::move(callback));
	}

	void OnCreateNewGroupTriggered(const QList<QModelIndex>&, const QModelIndex&, const IDataItem::Ptr&, Callback callback) const override
	{
		OnCreateNavigationItem(&ILogicFactory::CreateGroupController, std::move(callback));
	}

	void OnRenameGroupTriggered(const QList<QModelIndex>&, const QModelIndex& index, const IDataItem::Ptr&, Callback callback) const override
	{
		OnRenameNavigationItem(index, &ILogicFactory::CreateGroupController, std::move(callback));
	}

	void OnRemoveGroupTriggered(const QList<QModelIndex>& indexList, const QModelIndex& index, const IDataItem::Ptr&, Callback callback) const override
	{
		OnRemoveNavigationItem(indexList, index, &ILogicFactory::CreateGroupController, std::move(callback));
	}

	void OnCreateNewSearchTriggered(const QList<QModelIndex>&, const QModelIndex&, const IDataItem::Ptr&, Callback callback) const override
	{
		OnCreateNavigationItem(&ILogicFactory::CreateSearchController, std::move(callback));
	}

	void OnRemoveSearchTriggered(const QList<QModelIndex>& indexList, const QModelIndex& index, const IDataItem::Ptr&, Callback callback) const override
	{
		OnRemoveNavigationItem(indexList, index, &ILogicFactory::CreateSearchController, std::move(callback));
	}

	void OnAddToNewGroupTriggered(const QList<QModelIndex>& indexList, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const override
	{
		OnAddToGroupTriggered(indexList, index, item, std::move(callback));
	}

	void OnAddToGroupTriggered(const QList<QModelIndex>& indexList, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const override
	{
		ExecuteGroupActionImpl(&GroupController::AddToGroup, indexList, index, item, std::move(callback));
	}

	void OnRemoveFromGroupTriggered(const QList<QModelIndex>& indexList, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const override
	{
		ExecuteGroupActionImpl(&GroupController::RemoveFromGroup, indexList, index, item, std::move(callback));
	}

	void OnRemoveFromAllGroupsTriggered(const QList<QModelIndex>& indexList, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const override
	{
		OnRemoveFromGroupTriggered(indexList, index, item, std::move(callback));
	}

	void OnRemoveFromGroupOneItemTriggered(const QList<QModelIndex>&, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const override
	{
		ExecuteGroupActionInvertedImpl(&GroupController::RemoveFromGroup, index, item, std::move(callback));
	}

	void OnRemoveFromGroupAllBooksTriggered(const QList<QModelIndex>&, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const override
	{
		ExecuteGroupActionInvertedImpl(&GroupController::RemoveBooks, index, item, std::move(callback));
	}

	void OnRemoveFromGroupAllAuthorsTriggered(const QList<QModelIndex>&, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const override
	{
		ExecuteGroupActionInvertedImpl(&GroupController::RemoveAuthors, index, item, std::move(callback));
	}

	void OnRemoveFromGroupAllSeriesTriggered(const QList<QModelIndex>&, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const override
	{
		ExecuteGroupActionInvertedImpl(&GroupController::RemoveSeries, index, item, std::move(callback));
	}

	void OnRemoveFromGroupAllKeywordsTriggered(const QList<QModelIndex>&, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const override
	{
		ExecuteGroupActionInvertedImpl(&GroupController::RemoveKeywords, index, item, std::move(callback));
	}

	void OnAuthorReviewTriggered(const QList<QModelIndex>&, const QModelIndex& index, const IDataItem::Ptr&, Callback callback) const override
	{
		uiFactory->CreateAuthorReview(index.data(Role::Id).toLongLong());
		forwarder.Forward(std::move(callback));
	}

	void OnShowFilterSettingsTriggered(const QList<QModelIndex>&, const QModelIndex&, const IDataItem::Ptr&, Callback callback) const override
	{
		uiFactory->CreateFilterSettingsDialog()->exec();
		forwarder.Forward(std::move(callback));
	}

	void OnHideNavigationItemTriggered(const QList<QModelIndex>& indexList, const QModelIndex& index, const IDataItem::Ptr& /*item*/, Callback callback) const override
	{
		for (const auto& indexListItem : indexList)
			models[static_cast<size_t>(mode)]->setData(indexListItem, QVariant::fromValue(IDataItem::Flags::Filtered), Role::Flags);

		QStringList ids;
		std::ranges::move(GetSelected<QString>(index, indexList), std::back_inserter(ids));
		filterController->SetNavigationItemFlags(static_cast<NavigationMode>(mode), std::move(ids), IDataItem::Flags::Filtered, std::move(callback));
	}

	void OnFilterNavigationItemBooksTriggered(const QList<QModelIndex>& indexList, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const override
	{
		for (const auto& indexListItem : indexList)
			models[static_cast<size_t>(mode)]->setData(indexListItem, QVariant::fromValue(IDataItem::Flags::BooksFiltered), Role::Flags);

		QStringList ids;
		std::ranges::move(GetSelected<QString>(index, indexList), std::back_inserter(ids));

		const auto& str = item->GetData(MenuItem::Column::Checked);
		const auto checked = !str.isEmpty() && QVariant(str).toBool();
		const auto invoker = checked ? &IFilterController::ClearNavigationItemFlags : &IFilterController::SetNavigationItemFlags;

		std::invoke(invoker, *filterController, static_cast<NavigationMode>(mode), std::move(ids), IDataItem::Flags::BooksFiltered, std::move(callback));
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

	void OnUpdate(const std::string_view dbName, const std::string_view tableName, const int64_t rowId) override
	{
		OnInsert(dbName, tableName, rowId);
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
#define NAVIGATION_MODE_ITEM(NAME) OnTableChanged(NavigationMode::NAME);
		NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM
		self.RequestBooks(true);
	}

#define SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM(NAME, TYPE) \
	void On_##NAME##_Changed() override                      \
	{                                                        \
		OnTableChanged(NavigationMode::TYPE);                \
	}
	SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEMS_X_MACRO
#undef SUBSCRIBED_TABLES_RELOAD_NAVIGATION_ITEM

private: // INavigationFilter
	bool Stub(const ModeDescriptor&) const override
	{
		return true;
	}

	bool IsRecordExists(const ModeDescriptor& descriptor) const override
	{
		if (!descriptor.filterParameter)
			return true;

		auto db = databaseController->GetDatabase();
		if (!db)
			return true;

		const auto query = db->CreateQuery(descriptor.filterParameter);
		query->Execute();
		assert(!query->Eof());
		return query->Get<int>(0) != 0;
	}

	bool IsFolderExists(const ModeDescriptor& descriptor) const override
	{
		return descriptor.filterParameter && collectionProvider && collectionProvider->ActiveCollectionExists()
		    && QDir(collectionProvider->GetActiveCollection().folder + "/" + descriptor.filterParameter).exists();
	}

private: // IContextMenuProvider
	IDataItem::Ptr CreateAuthorsContextMenu(DB::IDatabase& db, const QString& id, const RequestContextMenuOptions options) override
	{
		auto result = MenuRequesterGroupNavigation(db, id, options);

		if (!result)
			return {};
		{
			const auto query = db.CreateQuery("select exists (select 42 from Reviews)");
			query->Execute();
			assert(!query->Eof());
			if (!query->Get<int>(0))
				return result;
		}

		const auto query =
			db.CreateQuery("select exists (select 42 from Reviews r join Books_View b on b.BookID = r.BookID and b.IsDeleted != ? join Author_List a on a.BookID = r.BookID and a.AuthorID = ?)");
		query->Bind(0, !(options & RequestContextMenuOptions::ShowRemoved) ? 1 : 2);
		query->Bind(1, id.toLongLong());
		query->Execute();
		assert(!query->Eof());

		AddMenuItem(result, QT_TRANSLATE_NOOP("Navigation", "Reviews"), MenuAction::AuthorReview)->SetData(QVariant(query->Get<int>(0) != 0).toString(), MenuItem::Column::Enabled);
		return result;
	}

	IDataItem::Ptr CreateSeriesContextMenu(DB::IDatabase& db, const QString& id, const RequestContextMenuOptions options) override
	{
		return MenuRequesterGroupNavigation(db, id, options);
	}

	IDataItem::Ptr CreateGenresContextMenu(DB::IDatabase& db, const QString& id, const RequestContextMenuOptions options) override
	{
		auto result = MenuRequesterFilter(db, id, options);
		return TreeMenuRequester(db, id, options, std::move(result));
	}

	IDataItem::Ptr CreatePublishYearContextMenu(DB::IDatabase& db, const QString& id, const RequestContextMenuOptions options) override
	{
		return TreeMenuRequester(db, id, options);
	}

	IDataItem::Ptr CreateKeywordsContextMenu(DB::IDatabase& db, const QString& id, const RequestContextMenuOptions options) override //-V524
	{
		return MenuRequesterGroupNavigation(db, id, options);
	}

	IDataItem::Ptr CreateUpdatesContextMenu(DB::IDatabase& db, const QString& id, const RequestContextMenuOptions options) override //-V524
	{
		return TreeMenuRequester(db, id, options);
	}

	IDataItem::Ptr CreateArchivesContextMenu(DB::IDatabase& /*db*/, const QString& /*id*/, RequestContextMenuOptions /*options*/) override
	{
		return {};
	}

	IDataItem::Ptr CreateLanguagesContextMenu(DB::IDatabase& db, const QString& id, const RequestContextMenuOptions options) override
	{
		return MenuRequesterFilter(db, id, options);
	}

	IDataItem::Ptr CreateGroupsContextMenu(DB::IDatabase& db, const QString& id, const RequestContextMenuOptions options) override
	{
		const auto hasSelection = !!(options & RequestContextMenuOptions::HasSelection);
		auto result = MenuItem::Create();

		AddMenuItem(result, QT_TRANSLATE_NOOP("Navigation", "Create new group..."), MenuAction::CreateNewGroup);
		AddMenuItem(result, QT_TRANSLATE_NOOP("Navigation", "Rename group..."), MenuAction::RenameGroup)->SetData(QVariant(hasSelection).toString(), MenuItem::Column::Enabled);
		AddMenuItem(result, REMOVE, MenuAction::RemoveGroup)->SetData(QVariant(hasSelection).toString(), MenuItem::Column::Enabled);
		auto removeFromGroup = AddMenuItem(result, QT_TRANSLATE_NOOP("Navigation", "Remove from group"));
		if (
			[&]
			{
				const auto query = db.CreateQuery("select exists (select 42 from Books b join Groups_List_User glu on glu.ObjectID = b.BookID and glu.GroupID = ?)");
				query->Bind(0, id.toInt());
				query->Execute();
				assert(!query->Eof());
				return query->Get<int>(0) != 0;
			}())
			AddMenuItem(removeFromGroup, QT_TRANSLATE_NOOP("Navigation", "All books"), MenuAction::RemoveFromGroupAllBooks);

		const auto addRemoveFromGroupItems = [&](const char* queryText, QString subMenuTitle, const int removeAllCommandId)
		{
			const auto query = db.CreateQuery(queryText);
			query->Bind(0, id.toInt());
			query->Execute();
			if (query->Eof())
				return;

			auto subMenu = AddMenuItem(removeFromGroup, std::move(subMenuTitle));
			for (; !query->Eof(); query->Next())
				AddMenuItem(subMenu, query->Get<const char*>(1), MenuAction::RemoveFromGroupOneItem)->SetData(QString::number(query->Get<long long>(0)), MenuItem::Column::Parameter);

			if (subMenu->GetChildCount() < 2)
				return;

			subMenu->SortChildren([](const IDataItem& lhs, const IDataItem& rhs) { return Util::QStringWrapper { lhs.GetData() } < Util::QStringWrapper { rhs.GetData() }; });

			AddMenuItem(subMenu);
			AddMenuItem(subMenu, QT_TRANSLATE_NOOP("Navigation", "All"), removeAllCommandId);
		};

		constexpr auto authorsQueryText =
			"select a.AuthorID, a.LastName || coalesce(' ' || nullif(a.FirstName, ''), '') || coalesce(' ' || nullif(a.MiddleName, ''), '') from Authors a join Groups_List_User glu on "
			"glu.ObjectID = a.AuthorID and glu.GroupID = ?";
		constexpr auto seriesQueryText = "select s.SeriesID, s.SeriesTitle from Series s join Groups_List_User glu on glu.ObjectID = s.SeriesID and glu.GroupID = ?";
		constexpr auto keywordsQueryText = "select k.KeywordID, k.KeywordTitle from Keywords k join Groups_List_User glu on glu.ObjectID = k.KeywordID and glu.GroupID = ?";

		addRemoveFromGroupItems(authorsQueryText, Loc::Authors, MenuAction::RemoveFromGroupAllAuthors);
		addRemoveFromGroupItems(seriesQueryText, Loc::Series, MenuAction::RemoveFromGroupAllSeries);
		addRemoveFromGroupItems(keywordsQueryText, Loc::Keywords, MenuAction::RemoveFromGroupAllKeywords);

		return result;
	}

	IDataItem::Ptr CreateSearchContextMenu(DB::IDatabase& /*db*/, const QString& /*id*/, const RequestContextMenuOptions options) override
	{
		auto result = MenuItem::Create();

		AddMenuItem(result, QT_TRANSLATE_NOOP("Navigation", "Create new search..."), MenuAction::CreateNewSearch);
		AddMenuItem(result, REMOVE, MenuAction::RemoveSearch)->SetData(QVariant(!!(options & RequestContextMenuOptions::HasSelection)).toString(), MenuItem::Column::Enabled);

		return result;
	}

	IDataItem::Ptr CreateReviewsContextMenu(DB::IDatabase& db, const QString& id, const RequestContextMenuOptions options) override
	{
		return TreeMenuRequester(db, id, options);
	}

	IDataItem::Ptr CreateAllBooksContextMenu(DB::IDatabase& /*db*/, const QString& /*id*/, RequestContextMenuOptions /*options*/) override
	{
		return {};
	}

private:
	template <typename T>
	using ControllerCreator = std::shared_ptr<T> (ILogicFactory::*)() const;

	template <typename T>
	void OnCreateNavigationItem(ControllerCreator<T> creator, Callback callback) const
	{
		auto controller = ((*ILogicFactory::Lock(logicFactory)).*creator)();
		auto& controllerRef = *controller;
		controllerRef.CreateNew(
			[controller = std::move(controller), callback = std::move(callback)](long long) mutable
			{
				callback();
				controller.reset();
			});
	}

	template <typename T>
	void OnRenameNavigationItem(const QModelIndex& index, ControllerCreator<T> creator, Callback callback) const
	{
		assert(index.isValid());
		auto controller = ((*ILogicFactory::Lock(logicFactory)).*creator)();
		auto& controllerRef = *controller;
		controllerRef.Rename(index.data(Role::Id).toLongLong(),
		                     index.data(Qt::DisplayRole).toString(),
		                     [controller = std::move(controller), callback = std::move(callback)](long long) mutable
		                     {
								 callback();
								 controller.reset();
							 });
	}

	template <typename T>
	void OnRemoveNavigationItem(const QList<QModelIndex>& indexList, const QModelIndex& index, ControllerCreator<T> creator, Callback callback) const
	{
		const auto toId = [](const QModelIndex& ind) { return ind.data(Role::Id).toLongLong(); };

		typename T::Ids ids;
		if (index.isValid())
			ids.emplace(toId(index));
		std::ranges::transform(indexList, std::inserter(ids, ids.end()), toId);
		if (ids.empty())
			return;

		auto controller = ((*ILogicFactory::Lock(logicFactory)).*creator)();
		auto& controllerRef = *controller;
		controllerRef.Remove(std::move(ids),
		                     [controller = std::move(controller), callback = std::move(callback)](long long) mutable
		                     {
								 callback();
								 controller.reset();
							 });
	}

	std::vector<std::pair<const char*, int>> GetModes() const
	{
		std::vector<std::pair<const char*, int>> result;
		std::ranges::transform(MODE_DESCRIPTORS
		                           | std::views::filter(
									   [&, &navigatorFilter = static_cast<const INavigationFilter&>(*this)](const auto& item)
									   {
										   return self.m_settings->Get(QString(Constant::Settings::VIEW_NAVIGATION_KEY_TEMPLATE).arg(item.first), true)
			                                   && std::invoke(item.second.filterInvoker, std::cref(navigatorFilter), std::cref(item.second));
									   }),
		                       std::back_inserter(result),
		                       [](const auto& item) { return std::make_pair(item.first, static_cast<int>(item.second.navigationMode)); });
		return result;
	}

	void ExecuteGroupActionImpl(const GroupActionFunction invoker, const QList<QModelIndex>& indexList, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const
	{
		const auto id = item->GetData(MenuItem::Column::Parameter).toLongLong();
		auto controller = ILogicFactory::Lock(logicFactory)->CreateGroupController();
		const auto& controllerRef = *controller;
		ExecuteGroupAction(controllerRef,
		                   invoker,
		                   id,
		                   GetSelected<GroupController::Id>(index, indexList),
		                   [callback = std::move(callback), controller = std::move(controller)](auto) mutable
		                   {
							   callback();
							   controller.reset();
						   });
	}

	void ExecuteGroupActionInvertedImpl(const GroupActionFunction invoker, const QModelIndex& index, const IDataItem::Ptr& item, Callback callback) const
	{
		auto controller = ILogicFactory::Lock(logicFactory)->CreateGroupController();
		const auto& controllerRef = *controller;
		ExecuteGroupAction(controllerRef,
		                   invoker,
		                   index.data(Role::Id).toLongLong(),
		                   { item->GetData(MenuItem::Column::Parameter).toLongLong() },
		                   [callback = std::move(callback), controller = std::move(controller)](auto) mutable
		                   {
							   callback();
							   controller.reset();
						   });
	}

	void OnTableChanged(const NavigationMode tableMode) const
	{
		static_cast<NavigationMode>(mode) == tableMode ? self.RequestNavigation(true) : models[static_cast<size_t>(tableMode)].reset();
	}

	static IDataItem::Ptr TreeMenuRequester(DB::IDatabase&, const QString&, const RequestContextMenuOptions options, IDataItem::Ptr result = {})
	{
		if (!(options & RequestContextMenuOptions::IsTree))
			return {};

		if (!result)
			result = MenuItem::Create();

		BooksContextMenuProvider::AddTreeMenuItems(result, options);
		return result;
	}

	IDataItem::Ptr MenuRequesterFilter(DB::IDatabase& db, const QString& id, const RequestContextMenuOptions options, IDataItem::Ptr result = {}) const
	{
		if (!(options & RequestContextMenuOptions::HasSelection))
			return {};

		if (!result)
			result = MenuItem::Create();

		const auto& description = IFilterController::GetFilteredNavigationDescription(static_cast<NavigationMode>(mode));
		assert(description.table && description.idField);
		const auto query = db.CreateQuery(std::format("select Flags from {} where {} = ?", description.table, description.idField));
		description.queueBinder(*query, 0, id);
		query->Execute();
		assert(!query->Eof());
		const auto booksFiltered = !!(static_cast<IDataItem::Flags>(query->Get<int>(0)) & IDataItem::Flags::BooksFiltered);

		const auto parent = AddMenuItem(result, QT_TRANSLATE_NOOP("Navigation", "Filters"));
		const auto filterEnabled = QVariant(!!(options & RequestContextMenuOptions::UniFilterEnabled)).toString();

		AddMenuItem(parent, QT_TRANSLATE_NOOP("Navigation", "Hide"), MenuAction::HideNavigationItem)->SetData(filterEnabled, MenuItem::Column::Enabled);
		AddMenuItem(parent, QT_TRANSLATE_NOOP("Navigation", "Filter books"), MenuAction::FilterNavigationItemBooks)
			->SetData(QVariant(true).toString(), MenuItem::Column::Checkable)
			.SetData(QVariant(booksFiltered).toString(), MenuItem::Column::Checked)
			.SetData(filterEnabled, MenuItem::Column::Enabled);
		AddMenuItem(parent, QT_TRANSLATE_NOOP("Navigation", "Filter settings..."), MenuAction::ShowFilterSettings);
		return result;
	}

	IDataItem::Ptr MenuRequesterGroupNavigation(DB::IDatabase& db, const QString& id, const RequestContextMenuOptions options, IDataItem::Ptr result = {}) const
	{
		if (!(options & RequestContextMenuOptions::HasSelection))
			return {};

		result = MenuRequesterFilter(db, id, options, std::move(result));

		CreateGroupMenu(result, id, db);
		return result;
	}

	NON_COPY_MOVABLE(Impl)
};

TreeViewControllerNavigation::TreeViewControllerNavigation(std::shared_ptr<ISettings> settings,
                                                           const std::shared_ptr<IModelProvider>& modelProvider,
                                                           const std::shared_ptr<const ILogicFactory>& logicFactory,
                                                           std::shared_ptr<const ICollectionProvider> collectionProvider,
                                                           std::shared_ptr<INavigationInfoProvider> dataProvider,
                                                           std::shared_ptr<IUiFactory> uiFactory,
                                                           std::shared_ptr<IDatabaseController> databaseController,
                                                           std::shared_ptr<IAuthorAnnotationController> authorAnnotationController,
                                                           std::shared_ptr<IFilterController> filterController)
	: AbstractTreeViewController(CONTEXT, std::move(settings), modelProvider)
	, m_impl(*this,
             logicFactory,
             std::move(collectionProvider),
             std::move(dataProvider),
             std::move(uiFactory),
             std::move(databaseController),
             std::move(authorAnnotationController),
             std::move(filterController))
{
	static_cast<ITreeViewController*>(this)->RegisterObserver(modelProvider.get());
	Setup();

	m_impl->dataProvider->SetNavigationRequestCallback(
		[&](IDataItem::Ptr data)
		{
			const auto modelCreator = MODE_DESCRIPTORS[m_impl->mode].second.modelCreator;
			auto model = std::invoke(modelCreator, IModelProvider::Lock(m_modelProvider), std::move(data), false);
			m_impl->models[m_impl->mode].reset(std::move(model));
			Perform(&IObserver::OnModelChanged, m_impl->models[m_impl->mode].get());
		});

	PLOGV << "TreeViewControllerNavigation created";
}

TreeViewControllerNavigation::~TreeViewControllerNavigation()
{
	if (auto modelProvider = m_modelProvider.lock())
		static_cast<ITreeViewController*>(this)->UnregisterObserver(modelProvider.get());
	PLOGV << "TreeViewControllerNavigation destroyed";
}

void TreeViewControllerNavigation::RequestNavigation(const bool force) const
{
	m_impl->dataProvider->RequestNavigation(force);
}

void TreeViewControllerNavigation::RequestBooks(const bool force) const
{
	m_impl->dataProvider->RequestBooks(force);
}

std::vector<std::pair<const char*, int>> TreeViewControllerNavigation::GetModeNames() const
{
	return m_impl->modes;
}

void TreeViewControllerNavigation::SetCurrentId(ItemType, QString id)
{
	m_impl->dataProvider->SetNavigationId(std::move(id));
}

const QString& TreeViewControllerNavigation::GetNavigationId() const noexcept
{
	return m_impl->dataProvider->GetNavigationID();
}

void TreeViewControllerNavigation::OnModeChanged(const QString& modeSrc)
{
	if (m_impl->modes.empty())
		return;

	QString mode = modeSrc;
	if (std::ranges::none_of(m_impl->modes, [&](const auto& item) { return item.first == mode; }))
		mode = m_impl->modes.front().first;

	m_impl->mode = GetModeIndex(mode);
	m_impl->dataProvider->SetNavigationMode(static_cast<NavigationMode>(m_impl->mode));
	m_impl->authorAnnotationController->SetNavigationMode(static_cast<NavigationMode>(m_impl->mode));
	Perform(&IObserver::OnModeChanged, m_impl->mode);

	if (m_impl->models[m_impl->mode])
		return Perform(&IObserver::OnModelChanged, m_impl->models[m_impl->mode].get());

	m_impl->dataProvider->RequestNavigation();
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

QString TreeViewControllerNavigation::GetItemName() const
{
	return Loc::NAVIGATION;
}

ViewMode TreeViewControllerNavigation::GetViewMode() const noexcept
{
	return MODE_DESCRIPTORS[m_impl->mode].second.viewMode;
}

void TreeViewControllerNavigation::RequestContextMenu(const QModelIndex& index, const RequestContextMenuOptions options, RequestContextMenuCallback callback)
{
	m_impl->RequestContextMenu(index, options, std::move(callback));
}

void TreeViewControllerNavigation::OnContextMenuTriggered(QAbstractItemModel*, const QModelIndex& index, const QList<QModelIndex>& indexList, const IDataItem::Ptr item) //-V801
{
	const auto invoker = FindSecond(MENU_HANDLERS, item->GetData(MenuItem::Column::Id).toInt(), &IContextMenuHandler::OnContextMenuTriggeredStub);
	std::invoke(invoker,
	            *m_impl,
	            std::cref(indexList),
	            std::cref(index),
	            std::cref(item),
	            [this, id = index.data(Role::Id).toString(), item] { Perform(&IObserver::OnContextMenuTriggered, std::cref(id), std::cref(item)); });
}

ITreeViewController::CreateNewItem TreeViewControllerNavigation::GetNewItemCreator() const
{
	if (const auto creator = MODE_DESCRIPTORS[m_impl->mode].second.createNewAction)
		return [creator, &impl = *m_impl] { std::invoke(creator, impl, QList<QModelIndex> {}, QModelIndex {}, IDataItem::Ptr {}, [] {}); };

	return {};
}

ITreeViewController::RemoveItems TreeViewControllerNavigation::GetRemoveItems() const
{
	if (const auto creator = MODE_DESCRIPTORS[m_impl->mode].second.createRemoveAction)
		return [creator, &impl = *m_impl](const QList<QModelIndex>& list)
		{
			assert(!list.empty());
			std::invoke(creator, impl, std::cref(list), list.front(), IDataItem::Ptr {}, [] {});
		};

	return {};
}
