#include "NavigationQueryExecutor.h"

#include <QHash>

#include <queue>
#include <ranges>
#include <unordered_map>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"

#include "data/Genre.h"
#include "database/DatabaseUtil.h"
#include "util/SortString.h"

#include "BooksTreeGenerator.h"
#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto AUTHORS_QUERY = "select AuthorID, FirstName, LastName, MiddleName, IsDeleted from Authors";
constexpr auto SERIES_QUERY = "select SeriesID, SeriesTitle, IsDeleted from Series";
constexpr auto KEYWORDS_QUERY = "select KeywordID, KeywordTitle, IsDeleted from Keywords";
constexpr auto GENRES_QUERY = "select g.GenreCode, g.GenreAlias, g.FB2Code, g.ParentCode, (select count(42) from Genre_List gl where gl.GenreCode = g.GenreCode) BookCount, IsDeleted from Genres g";
constexpr auto GROUPS_QUERY = "select GroupID, Title, IsDeleted from Groups_User";
constexpr auto UPDATES_QUERY = "select UpdateID, UpdateTitle, ParentId, IsDeleted from Updates order by ParentId";
constexpr auto ARCHIVES_QUERY = "select FolderID, FolderTitle, IsDeleted from Folders where exists (select 42 from Books where Books.FolderID = Folders.FolderID)";
constexpr auto LANGUAGES_QUERY = R"(with languages(lang) as (select distinct lang from Books)
	select l.lang, not exists (select 42 from Books b left join Books_User bu on bu.BookID = b.BookId where b.lang = l.lang and coalesce(bu.IsDeleted, b.IsDeleted, 0) = 0 ) IsDeleted
	from languages l)";
constexpr auto SEARCH_QUERY = "select SearchID, Title, 0 IsDeleted from Searches_User";
constexpr auto ALL_BOOK_QUERY = "select 'All books'";

constexpr auto WHERE_AUTHOR = "where a.AuthorID  = :id";
constexpr auto WHERE_GENRE = "where g.GenreCode = :id";
constexpr auto WHERE_UPDATE = "where b.UpdateID  = :id";
constexpr auto WHERE_ARCHIVE = "where b.FolderID  = :id";
constexpr auto WHERE_LANGUAGE = "where b.lang  = :id";
constexpr auto JOIN_SERIES = "join Series_List sl on sl.BookID = b.BookID and sl.SeriesID = :id";
constexpr auto JOIN_GROUPS = "join Groups_List_User grl on grl.BookID = b.BookID and grl.GroupID = :id";
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

IDataItem::Ptr CreateAuthorItem(const DB::IQuery& query, const size_t* index, const size_t removedIndex)
{
	auto item = NavigationItem::Create();

	item->SetId(QString::number(query.Get<long long>(index[0])));
	item->SetRemoved(query.Get<int>(removedIndex));
	item->SetData(CreateAuthorTitle(query, index));

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
	return query.Bind(":id", id.toStdString());
}

void RequestNavigationSimpleList(NavigationMode navigationMode, INavigationQueryExecutor::Callback callback, const IDatabaseUser& databaseUser, const QueryDescription& queryDescription, Cache& cache)
{
	auto db = databaseUser.Database();
	if (!db)
		return;

	databaseUser.Execute({ "Get navigation",
	                       [&queryDescription, &cache, mode = navigationMode, callback = std::move(callback), db = std::move(db)]() mutable
	                       {
							   std::unordered_map<QString, IDataItem::Ptr> index;

							   const auto query = db->CreateQuery(queryDescription.query);
							   for (query->Execute(); !query->Eof(); query->Next())
							   {
								   auto item = queryDescription.queryInfo.extractor(*query, queryDescription.queryInfo.index, queryDescription.queryInfo.removedIndex);
								   index.try_emplace(item->GetId(), item);
							   }

							   IDataItem::Items items;
							   items.reserve(std::size(index));
							   std::ranges::copy(index | std::views::values, std::back_inserter(items));

							   std::ranges::sort(items, [](const IDataItem::Ptr& lhs, const IDataItem::Ptr& rhs) { return Util::QStringWrapper { lhs->GetData() } < Util::QStringWrapper { rhs->GetData() }; });

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

void RequestNavigationGenres(NavigationMode navigationMode, INavigationQueryExecutor::Callback callback, const IDatabaseUser& databaseUser, const QueryDescription&, Cache& cache)
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

void RequestNavigationUpdates(NavigationMode navigationMode, INavigationQueryExecutor::Callback callback, const IDatabaseUser& databaseUser, const QueryDescription& queryDescription, Cache& cache)
{
	auto db = databaseUser.Database();
	if (!db)
		return;

	databaseUser.Execute({ "Get navigation",
	                       [&queryDescription, &cache, mode = navigationMode, callback = std::move(callback), db = std::move(db)]() mutable
	                       {
							   auto root = NavigationItem::Create();
							   std::unordered_map<long long, IDataItem::Ptr> items {
								   { 0, root },
							   };

							   const auto query = db->CreateQuery(queryDescription.query);
							   for (query->Execute(); !query->Eof(); query->Next())
							   {
								   auto item = queryDescription.queryInfo.extractor(*query, queryDescription.queryInfo.index, queryDescription.queryInfo.removedIndex);
								   const auto id = item->GetId().toLongLong();
								   const auto parentIt = items.find(query->Get<long long>(2));
								   assert(parentIt != items.end());
								   parentIt->second->AppendChild(items.try_emplace(id, std::move(item)).first->second);
							   }

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
						   } },
	                     1);
}

using NavigationRequest = void (*)(NavigationMode navigationMode, INavigationQueryExecutor::Callback callback, const IDatabaseUser& databaseUser, const QueryDescription& queryDescription, Cache& cache);

constexpr size_t NAVIGATION_QUERY_INDEX_AUTHOR[] { 0, 1, 2, 3 };
constexpr size_t NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM[] { 0, 1 };
constexpr size_t NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM_SINGLE[] { 0, 0 };
constexpr size_t NAVIGATION_QUERY_INDEX_GENRE_ITEM[] { 0, 1, 2 };

constexpr QueryInfo QUERY_INFO_AUTHOR { &CreateAuthorItem, NAVIGATION_QUERY_INDEX_AUTHOR, 4 };
constexpr QueryInfo QUERY_INFO_SIMPLE_LIST_ITEM { &DatabaseUtil::CreateSimpleListItem, NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM, 2 };
constexpr QueryInfo QUERY_INFO_LANGUAGE_ITEM { &DatabaseUtil::CreateLanguageItem, NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM_SINGLE, 1 };
constexpr QueryInfo QUERY_INFO_GENRE_ITEM { &DatabaseUtil::CreateGenreItem, NAVIGATION_QUERY_INDEX_GENRE_ITEM, 5 };
constexpr QueryInfo QUERY_INFO_UPDATE_ITEM { &DatabaseUtil::CreateSimpleListItem, NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM, 3 };

constexpr int MAPPING_FULL[] { BookItem::Column::Author, BookItem::Column::Title,    BookItem::Column::Series,  BookItem::Column::SeqNumber, BookItem::Column::Size,       BookItem::Column::Genre,
	                           BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate,  BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_AUTHORS[] { BookItem::Column::Title,    BookItem::Column::Series,  BookItem::Column::SeqNumber, BookItem::Column::Size,       BookItem::Column::Genre, BookItem::Column::Folder,
	                              BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate,  BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_SERIES[] { BookItem::Column::Author,   BookItem::Column::Title,   BookItem::Column::SeqNumber, BookItem::Column::Size,       BookItem::Column::Genre, BookItem::Column::Folder,
	                             BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate,  BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_GENRES[] { BookItem::Column::Author,   BookItem::Column::Title,   BookItem::Column::Series,   BookItem::Column::SeqNumber,  BookItem::Column::Size, BookItem::Column::Folder,
	                             BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };

constexpr int MAPPING_TREE_COMMON[] { BookItem::Column::Title,    BookItem::Column::SeqNumber, BookItem::Column::Size,     BookItem::Column::Genre,      BookItem::Column::Folder,
	                                  BookItem::Column::FileName, BookItem::Column::LibRate,   BookItem::Column::UserRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_TREE_GENRES[] { BookItem::Column::Title,   BookItem::Column::SeqNumber, BookItem::Column::Size,       BookItem::Column::Folder, BookItem::Column::FileName,
	                                  BookItem::Column::LibRate, BookItem::Column::UserRate,  BookItem::Column::UpdateDate, BookItem::Column::Lang };

constexpr std::pair<NavigationMode, std::pair<NavigationRequest, QueryDescription>> QUERIES[] {
	{   NavigationMode::Authors,
     { &RequestNavigationSimpleList,
     { AUTHORS_QUERY, QUERY_INFO_AUTHOR, WHERE_AUTHOR, nullptr, &BindInt, &IBooksTreeCreator::CreateAuthorsTree, BookItem::Mapping(MAPPING_AUTHORS), BookItem::Mapping(MAPPING_TREE_COMMON) } }           },
	{    NavigationMode::Series,
     { &RequestNavigationSimpleList,
     { SERIES_QUERY,
     QUERY_INFO_SIMPLE_LIST_ITEM,
     nullptr,
     JOIN_SERIES,
     &BindInt,
     &IBooksTreeCreator::CreateSeriesTree,
     BookItem::Mapping(MAPPING_SERIES),
     BookItem::Mapping(MAPPING_TREE_COMMON),
     "sl" } }																																															 },
	{    NavigationMode::Genres,
     { &RequestNavigationGenres,
     { GENRES_QUERY, QUERY_INFO_GENRE_ITEM, WHERE_GENRE, nullptr, &BindString, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_GENRES), BookItem::Mapping(MAPPING_TREE_GENRES) } }       },
	{  NavigationMode::Keywords,
     { &RequestNavigationSimpleList,
     { KEYWORDS_QUERY, QUERY_INFO_SIMPLE_LIST_ITEM, nullptr, JOIN_KEYWORDS, &BindInt, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL), BookItem::Mapping(MAPPING_TREE_COMMON) } }  },
	{    NavigationMode::Groups,
     { &RequestNavigationSimpleList,
     { GROUPS_QUERY, QUERY_INFO_SIMPLE_LIST_ITEM, nullptr, JOIN_GROUPS, &BindInt, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL), BookItem::Mapping(MAPPING_TREE_COMMON) } }      },
	{   NavigationMode::Updates,
     { &RequestNavigationUpdates,
     { UPDATES_QUERY, QUERY_INFO_UPDATE_ITEM, WHERE_UPDATE, nullptr, &BindInt, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL), BookItem::Mapping(MAPPING_TREE_COMMON) } }         },
	{  NavigationMode::Archives,
     { &RequestNavigationSimpleList,
     { ARCHIVES_QUERY, QUERY_INFO_SIMPLE_LIST_ITEM, WHERE_ARCHIVE, nullptr, &BindInt, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL), BookItem::Mapping(MAPPING_TREE_COMMON) } }  },
	{ NavigationMode::Languages,
     { &RequestNavigationSimpleList,
     { LANGUAGES_QUERY, QUERY_INFO_LANGUAGE_ITEM, WHERE_LANGUAGE, nullptr, &BindString, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL), BookItem::Mapping(MAPPING_TREE_COMMON) } } },
	{    NavigationMode::Search,
     { &RequestNavigationSimpleList,
     { SEARCH_QUERY, QUERY_INFO_SIMPLE_LIST_ITEM, nullptr, JOIN_SEARCHES, &BindInt, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL), BookItem::Mapping(MAPPING_TREE_COMMON), "b", WITH_SEARCH } }    },
	{  NavigationMode::AllBooks,
     { &RequestNavigationSimpleList,
     { ALL_BOOK_QUERY, QUERY_INFO_SIMPLE_LIST_ITEM, nullptr, nullptr, &BindStub, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL), BookItem::Mapping(MAPPING_TREE_COMMON) } }       },
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
	mutable Cache cache;

	explicit Impl(std::shared_ptr<const IDatabaseUser> databaseUser)
		: databaseUser(std::move(databaseUser))
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

NavigationQueryExecutor::NavigationQueryExecutor(std::shared_ptr<IDatabaseUser> databaseUser)
	: m_impl(std::move(databaseUser))
{
	PLOGV << "NavigationQueryExecutor created";
}

NavigationQueryExecutor::~NavigationQueryExecutor()
{
	PLOGV << "NavigationQueryExecutor destroyed";
}

void NavigationQueryExecutor::RequestNavigation(const NavigationMode navigationMode, Callback callback, const bool force) const
{
	if (const auto it = m_impl->cache.find(navigationMode); it != m_impl->cache.end())
	{
		if (force)
			m_impl->cache.erase(it);
		else
			return callback(navigationMode, it->second);
	}

	const auto& [request, description] = FindSecond(QUERIES, navigationMode);
	(*request)(navigationMode, std::move(callback), *m_impl->databaseUser, description, m_impl->cache);
}

const QueryDescription& NavigationQueryExecutor::GetQueryDescription(const NavigationMode navigationMode) const
{
	const auto& [_, description] = FindSecond(QUERIES, navigationMode);
	return description;
}
