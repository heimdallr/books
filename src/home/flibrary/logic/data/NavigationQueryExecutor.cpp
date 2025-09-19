#include "NavigationQueryExecutor.h"

#include <queue>
#include <ranges>
#include <unordered_map>

#include <QDir>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/logic/ICollectionProvider.h"

#include "data/Genre.h"
#include "database/DatabaseUtil.h"
#include "inpx/src/util/constant.h"
#include "util/SortString.h"

#include "BooksTreeGenerator.h"
#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto AUTHORS_QUERY = "select AuthorID, FirstName, LastName, MiddleName, IsDeleted, Flags from Authors";
constexpr auto SERIES_QUERY = "select SeriesID, SeriesTitle, IsDeleted, Flags from Series";
constexpr auto KEYWORDS_QUERY = "select KeywordID, KeywordTitle, IsDeleted, Flags from Keywords";
constexpr auto GENRES_QUERY = "select g.GenreCode, g.GenreAlias, g.FB2Code, g.ParentCode, (select count(42) from Genre_List gl where gl.GenreCode = g.GenreCode) BookCount, IsDeleted, Flags from Genres g";
constexpr auto GROUPS_QUERY = "select GroupID, Title, IsDeleted from Groups_User";
constexpr auto UPDATES_QUERY = "select UpdateID, UpdateTitle, ParentId, IsDeleted, Flags from Updates order by ParentId";
constexpr auto ARCHIVES_QUERY = "select FolderID, FolderTitle, IsDeleted, Flags from Folders where exists (select 42 from Books where Books.FolderID = Folders.FolderID)";
constexpr auto LANGUAGES_QUERY = "select l.LanguageCode, not exists (select 42 from Books_View b where b.Lang = l.LanguageCode and b.IsDeleted = 0 ) IsDeleted, Flags from Languages l";
constexpr auto PUBLISH_YEAR_QUERY = R"(with PublishYears(Year) as (select distinct Year from Books)
	select y.Year, y.Year, not exists (select 42 from Books_View b where b.Year = y.Year and b.IsDeleted = 0 ) IsDeleted, 0 Flags
	from PublishYears y)";
constexpr auto SEARCH_QUERY = "select SearchID, Title, 0 IsDeleted, 0 Flags from Searches_User";
constexpr auto ALL_BOOK_QUERY = "select 'All books'";

constexpr auto WHERE_AUTHOR = "where a.AuthorID  = :id";
constexpr auto WHERE_GENRE = "where g.GenreCode = :id";
constexpr auto WHERE_UPDATE = "where b.UpdateID  = :id";
constexpr auto WHERE_ARCHIVE = "where b.FolderID  = :id";
constexpr auto WHERE_LANGUAGE = "where b.Lang  = :id";
constexpr auto WHERE_YEAR = "where b.Year  = :id";
constexpr auto JOIN_SERIES = "join Series_List sl on sl.BookID = b.BookID and sl.SeriesID = :id";
constexpr auto JOIN_GROUPS = "join Groups_List_User_View grl on grl.BookID = b.BookID and grl.GroupID = :id";
constexpr auto JOIN_SEARCHES = "join Ids i on i.BookID = b.BookID";
constexpr auto JOIN_KEYWORDS = "join Keyword_List kl on kl.BookID = b.BookID and kl.KeywordID = :id";

constexpr auto WITH_SEARCH = R"(
with Ids(BookID) as (
    with Search (Title) as (
        select Title
        from Searches_User where SearchID = :id
    )
    select b.BookID
        from Books b
        join Books_Search fts on fts.rowid = b.BookID
        join Search s on Books_Search match s.Title
    union
    select b.BookID
        from Books b
        join Author_List l on l.BookID = b.BookID
        join Authors_Search fts on fts.rowid = l.AuthorID
        join Search s on Authors_Search match s.Title
    union
    select b.BookID
        from Books b
        join Series_List l on l.BookID = b.BookID
        join Series_Search fts on fts.rowid = l.SeriesID
        join Search s on Series_Search match s.Title
)
)";

using Cache = std::unordered_map<NavigationMode, IDataItem::Ptr>;

QString CreateAuthorTitle(const DB::IQuery& query, const size_t* index)
{
	QString title = query.Get<const char*>(index[2]);
	AppendTitle(title, query.Get<const char*>(index[1]));
	AppendTitle(title, query.Get<const char*>(index[3]));

	if (title.isEmpty())
		title = Loc::Tr(Loc::Ctx::ERROR, Loc::AUTHOR_NOT_SPECIFIED);

	return title;
}

IDataItem::Ptr CreateAuthorItem(const DB::IQuery& query, const QueryInfo& queryInfo)
{
	auto item = NavigationItem::Create();

	item->SetId(QString::number(query.Get<long long>(queryInfo.index[0])));
	item->SetFlags(static_cast<IDataItem::Flags>(query.Get<int>(queryInfo.flagsIndex)));
	item->SetRemoved(query.Get<int>(queryInfo.removedIndex));
	item->SetData(CreateAuthorTitle(query, queryInfo.index));

	return item;
}

int BindStub(DB::IQuery& /*query*/, const QString& /*id*/)
{
	return 0;
}

int BindInt(DB::IQuery& query, const QString& id)
{
	return query.Bind(":id", id.toInt());
}

int BindString(DB::IQuery& query, const QString& id)
{
	return id.isEmpty() ? query.Bind(":id") : query.Bind(":id", id.toStdString());
}

auto CreateCalendarTree(const NavigationMode mode, INavigationQueryExecutor::Callback callback, Cache& cache, const auto& selector)
{
	auto root = NavigationItem::Create();
	std::unordered_map<long long, IDataItem::Ptr> items {
		{ 0, root },
	};

	selector(items);

	const auto updateChildren = [](IDataItem& item, const auto& f) -> void
	{
		item.SortChildren([](const IDataItem& lhs, const IDataItem& rhs) { return lhs.GetData().toLongLong() < rhs.GetData().toLongLong(); });
		for (size_t i = 0, sz = item.GetChildCount(); i < sz; ++i)
		{
			auto child = item.GetChild(i);
			child->SetData(Loc::Tr(MONTHS_CONTEXT, child->GetData().toStdString().data()));
			f(*child, f);
		}
	};

	updateChildren(*root, updateChildren);

	return [&cache, mode, callback = std::move(callback), root = std::move(root)](size_t) mutable
	{
		cache[mode] = root;
		callback(mode, std::move(root));
	};
}

void RequestNavigationList(const NavigationMode navigationMode,
                           INavigationQueryExecutor::Callback callback,
                           const IDatabaseUser& databaseUser,
                           const QueryDescription& queryDescription,
                           Cache& cache,
                           std::function<void(IDataItem::Items&)> postProcess)
{
	auto db = databaseUser.Database();
	if (!db)
		return;

	databaseUser.Execute({ "Get navigation",
	                       [&queryDescription, &cache, mode = navigationMode, callback = std::move(callback), postProcess = std::move(postProcess), db = std::move(db)]() mutable
	                       {
							   std::unordered_map<QString, IDataItem::Ptr> index;

							   const auto query = db->CreateQuery(queryDescription.query);
							   for (query->Execute(); !query->Eof(); query->Next())
							   {
								   auto item = queryDescription.queryInfo.extractor(*query, queryDescription.queryInfo);
								   index.try_emplace(item->GetId(), item);
							   }

							   IDataItem::Items items;
							   items.reserve(std::size(index));
							   std::ranges::copy(index | std::views::values, std::back_inserter(items));

							   postProcess(items);

							   auto root = NavigationItem::Create();
							   root->SetChildren(std::move(items));

							   return [&cache, mode, callback = std::move(callback), root = std::move(root)](size_t) mutable
							   {
								   cache[mode] = root;
								   callback(mode, std::move(root));
							   };
						   } },
	                     1);
}

void RequestNavigationSimpleList(const NavigationMode navigationMode,
                                 INavigationQueryExecutor::Callback callback,
                                 const IDatabaseUser& databaseUser,
                                 const QueryDescription& queryDescription,
                                 const ICollectionProvider&,
                                 Cache& cache)
{
	RequestNavigationList(
		navigationMode,
		std::move(callback),
		databaseUser,
		queryDescription,
		cache,
		[](IDataItem::Items& items)
		{ std::ranges::sort(items, [](const IDataItem::Ptr& lhs, const IDataItem::Ptr& rhs) { return Util::QStringWrapper { lhs->GetData() } < Util::QStringWrapper { rhs->GetData() }; }); });
}

void RequestNavigationPublishYears(const NavigationMode navigationMode,
                                   INavigationQueryExecutor::Callback callback,
                                   const IDatabaseUser& databaseUser,
                                   const QueryDescription& queryDescription,
                                   const ICollectionProvider&,
                                   Cache& cache)
{
	RequestNavigationList(navigationMode,
	                      std::move(callback),
	                      databaseUser,
	                      queryDescription,
	                      cache,
	                      [](IDataItem::Items& items)
	                      {
							  auto getYear = [](const IDataItem& item) -> int
							  {
								  const auto& id = item.GetId();
								  if (id.isEmpty())
									  return std::numeric_limits<int>::max();

								  bool ok = false;
								  const auto year = id.toInt(&ok);
								  return !ok ? std::numeric_limits<int>::max() : year < 500 ? year + 10000 : year;
							  };
							  std::ranges::sort(items, [getYear = std::move(getYear)](const IDataItem::Ptr& lhs, const IDataItem::Ptr& rhs) { return getYear(*lhs) < getYear(*rhs); });
							  if (const auto it = std::ranges::find_if(items, [](const auto& item) { return item->GetId().isEmpty(); }); it != std::cend(items))
								  (*it)->SetData(Loc::Tr(LANGUAGES_CONTEXT, UNDEFINED));
						  });
}

void RequestNavigationGenres(NavigationMode navigationMode, INavigationQueryExecutor::Callback callback, const IDatabaseUser& databaseUser, const QueryDescription&, const ICollectionProvider&, Cache& cache)
{
	auto db = databaseUser.Database();
	if (!db)
		return;

	databaseUser.Execute({ "Get navigation",
	                       [&cache, mode = navigationMode, callback = std::move(callback), db = std::move(db)]() mutable
	                       {
							   auto genre = Genre::Load(*db);
							   auto root = NavigationItem::Create();
							   std::queue<std::pair<const Genre*, IDataItem*>> queue;
							   queue.emplace(&genre, root.get());
							   while (!queue.empty())
							   {
								   auto [genreItem, dataItem] = queue.front();
								   queue.pop();

								   dataItem->SetId(genreItem->code);
								   dataItem->SetFlags(genreItem->flags);
								   dataItem->SetRemoved(genreItem->removed);
								   dataItem->SetData(genreItem->name);
								   for (const auto& item : genreItem->children)
								   {
									   auto& ptr = dataItem->AppendChild(NavigationItem::Create());
									   queue.emplace(&item, ptr.get());
								   }
							   }

							   return [&cache, mode, callback = std::move(callback), root = std::move(root)](size_t) mutable
							   {
								   cache[mode] = root;
								   callback(mode, std::move(root));
							   };
						   } },
	                     1);
}

void RequestNavigationUpdates(NavigationMode navigationMode,
                              INavigationQueryExecutor::Callback callback,
                              const IDatabaseUser& databaseUser,
                              const QueryDescription& queryDescription,
                              const ICollectionProvider&,
                              Cache& cache)
{
	auto db = databaseUser.Database();
	if (!db)
		return;

	databaseUser.Execute({ "Get navigation",
	                       [&queryDescription, &cache, mode = navigationMode, callback = std::move(callback), db = std::move(db)]() mutable
	                       {
							   return CreateCalendarTree(mode,
		                                                 std::move(callback),
		                                                 cache,
		                                                 [&queryDescription, db = std::move(db)](std::unordered_map<long long, IDataItem::Ptr>& items)
		                                                 {
															 const auto query = db->CreateQuery(queryDescription.query);
															 for (query->Execute(); !query->Eof(); query->Next())
															 {
																 auto item = queryDescription.queryInfo.extractor(*query, queryDescription.queryInfo);
																 const auto id = item->GetId().toLongLong();
																 const auto parentIt = items.find(query->Get<long long>(2));
																 assert(parentIt != items.end());
																 parentIt->second->AppendChild(items.try_emplace(id, std::move(item)).first->second);
															 }
														 });
						   } },
	                     1);
}

void RequestNavigationReviews(NavigationMode navigationMode,
                              INavigationQueryExecutor::Callback callback,
                              const IDatabaseUser& databaseUser,
                              const QueryDescription& /*queryDescription*/,
                              const ICollectionProvider& collectionProvider,
                              Cache& cache)
{
	assert(collectionProvider.ActiveCollectionExists());

	databaseUser.Execute({ "Get navigation",
	                       [&cache, mode = navigationMode, folder = collectionProvider.GetActiveCollection().folder, callback = std::move(callback)]() mutable
	                       {
							   return CreateCalendarTree(mode,
		                                                 std::move(callback),
		                                                 cache,
		                                                 [&folder](std::unordered_map<long long, IDataItem::Ptr>& items)
		                                                 {
															 for (const auto& reviewInfo : QDir(folder + "/" + QString::fromStdWString(REVIEWS_FOLDER)).entryInfoList({ "??????.7z" }))
															 {
																 auto name = reviewInfo.completeBaseName();
																 auto year = name.first(4);
																 const auto yearId = year.toLongLong(), monthId = name.last(2).toLongLong();
																 auto parentIt = items.find(yearId);
																 if (parentIt == items.end())
																 {
																	 auto parent = items.at(0)->AppendChild(NavigationItem::Create());
																	 parent->SetId(year);
																	 parent->SetData(year, NavigationItem::Column::Title);
																	 parentIt = items.try_emplace(yearId, std::move(parent)).first;
																 }

																 auto item = parentIt->second->AppendChild(NavigationItem::Create());
																 item->SetId(std::move(name));
																 item->SetData(QString::number(monthId), NavigationItem::Column::Title);
																 items.try_emplace(yearId * 10000LL + monthId, std::move(item));
															 }
														 });
						   } },
	                     1);
}

using NavigationRequest =
	void (*)(NavigationMode navigationMode, INavigationQueryExecutor::Callback callback, const IDatabaseUser& databaseUser, const QueryDescription& queryDescription, const ICollectionProvider&, Cache& cache);

constexpr size_t NAVIGATION_QUERY_INDEX_AUTHOR[] { 0, 1, 2, 3 };
constexpr size_t NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM[] { 0, 1 };
constexpr size_t NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM_SINGLE[] { 0, 0 };
constexpr size_t NAVIGATION_QUERY_INDEX_GENRE_ITEM[] { 0, 1, 2 };

constexpr QueryInfo QUERY_INFO_AUTHOR { &CreateAuthorItem, NAVIGATION_QUERY_INDEX_AUTHOR, 4, 5 };
constexpr QueryInfo QUERY_INFO_SIMPLE_LIST_ITEM { &DatabaseUtil::CreateSimpleListItem, NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM, 2, 3 };
constexpr QueryInfo QUERY_INFO_LANGUAGE_ITEM { &DatabaseUtil::CreateLanguageItem, NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM_SINGLE, 1, 2 };
constexpr QueryInfo QUERY_INFO_GENRE_ITEM { &DatabaseUtil::CreateGenreItem, NAVIGATION_QUERY_INDEX_GENRE_ITEM, 5, 6 };
constexpr QueryInfo QUERY_INFO_UPDATE_ITEM { &DatabaseUtil::CreateSimpleListItem, NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM, 3, 4 };

constexpr int MAPPING_FULL[] { BookItem::Column::Author,     BookItem::Column::Title,  BookItem::Column::Series,   BookItem::Column::SeqNumber, BookItem::Column::Size,
	                           BookItem::Column::Genre,      BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate,   BookItem::Column::UserRate,
	                           BookItem::Column::UpdateDate, BookItem::Column::Year,   BookItem::Column::Lang };
constexpr int MAPPING_AUTHORS[] { BookItem::Column::Title,    BookItem::Column::Series,  BookItem::Column::SeqNumber, BookItem::Column::Size,       BookItem::Column::Genre, BookItem::Column::Folder,
	                              BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate,  BookItem::Column::UpdateDate, BookItem::Column::Year,  BookItem::Column::Lang };
constexpr int MAPPING_SERIES[] { BookItem::Column::Author,   BookItem::Column::Title,   BookItem::Column::SeqNumber, BookItem::Column::Size,       BookItem::Column::Genre, BookItem::Column::Folder,
	                             BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate,  BookItem::Column::UpdateDate, BookItem::Column::Year,  BookItem::Column::Lang };
constexpr int MAPPING_GENRES[] { BookItem::Column::Author,   BookItem::Column::Title,   BookItem::Column::Series,   BookItem::Column::SeqNumber,  BookItem::Column::Size, BookItem::Column::Folder,
	                             BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate, BookItem::Column::UpdateDate, BookItem::Column::Year, BookItem::Column::Lang };

constexpr int MAPPING_TREE_COMMON[] { BookItem::Column::Title,   BookItem::Column::SeqNumber, BookItem::Column::Size,       BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName,
	                                  BookItem::Column::LibRate, BookItem::Column::UserRate,  BookItem::Column::UpdateDate, BookItem::Column::Year,  BookItem::Column::Lang };
constexpr int MAPPING_TREE_GENRES[] { BookItem::Column::Title,   BookItem::Column::SeqNumber, BookItem::Column::Size,       BookItem::Column::Folder, BookItem::Column::FileName,
	                                  BookItem::Column::LibRate, BookItem::Column::UserRate,  BookItem::Column::UpdateDate, BookItem::Column::Year,   BookItem::Column::Lang };

constexpr std::pair<NavigationMode, std::pair<NavigationRequest, QueryDescription>> QUERIES[] {
	{     NavigationMode::Authors,
     { &RequestNavigationSimpleList,
     { AUTHORS_QUERY,
     QUERY_INFO_AUTHOR,
     WHERE_AUTHOR,
     nullptr,
     &BindInt,
     &IBooksListCreator::CreateGeneralList,
     &IBooksTreeCreator::CreateAuthorsTree,
     BookItem::Mapping(MAPPING_AUTHORS),
     BookItem::Mapping(MAPPING_TREE_COMMON) } } },
	{	  NavigationMode::Series,
     { &RequestNavigationSimpleList,
     { SERIES_QUERY,
     QUERY_INFO_SIMPLE_LIST_ITEM,
     nullptr,
     JOIN_SERIES,
     &BindInt,
     &IBooksListCreator::CreateGeneralList,
     &IBooksTreeCreator::CreateSeriesTree,
     BookItem::Mapping(MAPPING_SERIES),
     BookItem::Mapping(MAPPING_TREE_COMMON),
     "sl" } }								  },
	{	  NavigationMode::Genres,
     { &RequestNavigationGenres,
     { GENRES_QUERY,
     QUERY_INFO_GENRE_ITEM,
     WHERE_GENRE,
     nullptr,
     &BindString,
     &IBooksListCreator::CreateGeneralList,
     &IBooksTreeCreator::CreateGeneralTree,
     BookItem::Mapping(MAPPING_GENRES),
     BookItem::Mapping(MAPPING_TREE_GENRES) } } },
	{ NavigationMode::PublishYear,
     { &RequestNavigationPublishYears,
     { PUBLISH_YEAR_QUERY,
     QUERY_INFO_SIMPLE_LIST_ITEM,
     WHERE_YEAR,
     nullptr,
     &BindString,
     &IBooksListCreator::CreateGeneralList,
     &IBooksTreeCreator::CreateGeneralTree,
     BookItem::Mapping(MAPPING_FULL),
     BookItem::Mapping(MAPPING_TREE_COMMON) } } },
	{    NavigationMode::Keywords,
     { &RequestNavigationSimpleList,
     { KEYWORDS_QUERY,
     QUERY_INFO_SIMPLE_LIST_ITEM,
     nullptr,
     JOIN_KEYWORDS,
     &BindInt,
     &IBooksListCreator::CreateGeneralList,
     &IBooksTreeCreator::CreateGeneralTree,
     BookItem::Mapping(MAPPING_FULL),
     BookItem::Mapping(MAPPING_TREE_COMMON) } } },
	{	  NavigationMode::Groups,
     { &RequestNavigationSimpleList,
     { GROUPS_QUERY,
     QUERY_INFO_SIMPLE_LIST_ITEM,
     nullptr,
     JOIN_GROUPS,
     &BindInt,
     &IBooksListCreator::CreateGeneralList,
     &IBooksTreeCreator::CreateGeneralTree,
     BookItem::Mapping(MAPPING_FULL),
     BookItem::Mapping(MAPPING_TREE_COMMON) } } },
	{     NavigationMode::Updates,
     { &RequestNavigationUpdates,
     { UPDATES_QUERY,
     QUERY_INFO_UPDATE_ITEM,
     WHERE_UPDATE,
     nullptr,
     &BindInt,
     &IBooksListCreator::CreateGeneralList,
     &IBooksTreeCreator::CreateGeneralTree,
     BookItem::Mapping(MAPPING_FULL),
     BookItem::Mapping(MAPPING_TREE_COMMON) } } },
	{    NavigationMode::Archives,
     { &RequestNavigationSimpleList,
     { ARCHIVES_QUERY,
     QUERY_INFO_SIMPLE_LIST_ITEM,
     WHERE_ARCHIVE,
     nullptr,
     &BindInt,
     &IBooksListCreator::CreateGeneralList,
     &IBooksTreeCreator::CreateGeneralTree,
     BookItem::Mapping(MAPPING_FULL),
     BookItem::Mapping(MAPPING_TREE_COMMON) } } },
	{   NavigationMode::Languages,
     { &RequestNavigationSimpleList,
     { LANGUAGES_QUERY,
     QUERY_INFO_LANGUAGE_ITEM,
     WHERE_LANGUAGE,
     nullptr,
     &BindString,
     &IBooksListCreator::CreateGeneralList,
     &IBooksTreeCreator::CreateGeneralTree,
     BookItem::Mapping(MAPPING_FULL),
     BookItem::Mapping(MAPPING_TREE_COMMON) } } },
	{	  NavigationMode::Search,
     { &RequestNavigationSimpleList,
     { SEARCH_QUERY,
     QUERY_INFO_SIMPLE_LIST_ITEM,
     nullptr,
     JOIN_SEARCHES,
     &BindInt,
     &IBooksListCreator::CreateGeneralList,
     &IBooksTreeCreator::CreateGeneralTree,
     BookItem::Mapping(MAPPING_FULL),
     BookItem::Mapping(MAPPING_TREE_COMMON),
     "b",
     WITH_SEARCH } }						   },
	{     NavigationMode::Reviews,
     { &RequestNavigationReviews,
     { nullptr,
     {},
     nullptr,
     nullptr,
     nullptr,
     &IBooksListCreator::CreateReviewsList,
     &IBooksTreeCreator::CreateReviewsTree,
     BookItem::Mapping(MAPPING_FULL),
     BookItem::Mapping(MAPPING_FULL),
     "b",
     nullptr,
     &IBookSelector::SelectReviews } }         },
	{    NavigationMode::AllBooks,
     { &RequestNavigationSimpleList,
     { ALL_BOOK_QUERY,
     QUERY_INFO_SIMPLE_LIST_ITEM,
     nullptr,
     nullptr,
     &BindStub,
     &IBooksListCreator::CreateGeneralList,
     &IBooksTreeCreator::CreateGeneralTree,
     BookItem::Mapping(MAPPING_FULL),
     BookItem::Mapping(MAPPING_TREE_COMMON) } } },
};

static_assert(static_cast<size_t>(NavigationMode::Last) == std::size(QUERIES));

constexpr std::pair<const char*, NavigationMode> TABLES[] {
	{	   "Authors",  NavigationMode::Authors },
    {        "Series",   NavigationMode::Series },
    {        "Genres",   NavigationMode::Genres },
    {      "Keywords", NavigationMode::Keywords },
	{   "Groups_User",   NavigationMode::Groups },
    {         "Books", NavigationMode::Archives },
    { "Searches_User",   NavigationMode::Search },
    {       "Updates",  NavigationMode::Updates },
};

} // namespace

struct NavigationQueryExecutor::Impl final : virtual DB::IDatabaseObserver
{
	std::shared_ptr<const IDatabaseUser> databaseUser;
	std::shared_ptr<const ICollectionProvider> collectionProvider;
	mutable Cache cache;

	Impl(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const ICollectionProvider> collectionProvider)
		: databaseUser { std::move(databaseUser) }
		, collectionProvider { std::move(collectionProvider) }
	{
		if (const auto db = this->databaseUser->Database())
			db->RegisterObserver(this);
	}

	~Impl() override
	{
		if (const auto db = this->databaseUser->CheckDatabase())
			db->UnregisterObserver(this);
	}

private: // DB::IDatabaseObserver
	void OnInsert(std::string_view, const std::string_view tableName, int64_t) override
	{
		OnTableUpdate(tableName);
	}

	void OnUpdate(std::string_view, std::string_view, int64_t) override
	{
	}

	void OnDelete(const std::string_view dbName, const std::string_view tableName, const int64_t id) override
	{
		OnInsert(dbName, tableName, id);
	}

private:
	void OnTableUpdate(const std::string_view tableName) const
	{
		if (const auto it = FindPairIteratorByFirst(TABLES, tableName.data(), PszComparer {}); it != std::cend(TABLES))
			cache.erase(it->second);
	}

	NON_COPY_MOVABLE(Impl)
};

NavigationQueryExecutor::NavigationQueryExecutor(std::shared_ptr<IDatabaseUser> databaseUser, std::shared_ptr<const ICollectionProvider> collectionProvider)
	: m_impl(std::move(databaseUser), std::move(collectionProvider))
{
	PLOGV << "NavigationQueryExecutor created";
}

NavigationQueryExecutor::~NavigationQueryExecutor()
{
	PLOGV << "NavigationQueryExecutor destroyed";
}

void NavigationQueryExecutor::RequestNavigation(const NavigationMode navigationMode, Callback callback, const bool force) const
{
	auto& cache = m_impl->cache;
	if (const auto it = cache.find(navigationMode); it != cache.end())
	{
		if (force)
			cache.erase(it);
		else
			return callback(navigationMode, it->second);
	}

	const auto& [request, description] = FindSecond(QUERIES, navigationMode);
	(*request)(navigationMode, std::move(callback), *m_impl->databaseUser, description, *m_impl->collectionProvider, cache);
}

const QueryDescription& NavigationQueryExecutor::GetQueryDescription(const NavigationMode navigationMode) const
{
	const auto& [_, description] = FindSecond(QUERIES, navigationMode);
	return description;
}
