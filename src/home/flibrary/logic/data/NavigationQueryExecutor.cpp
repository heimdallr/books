#include "NavigationQueryExecutor.h"

#include <ranges>
#include <stack>

#include "fnd/FindPair.h"
#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "shared/DatabaseUser.h"
#include "BooksTreeGenerator.h"

#include <plog/Log.h>

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto AUTHORS_QUERY = "select AuthorID, FirstName, LastName, MiddleName from Authors";
constexpr auto SERIES_QUERY = "select SeriesID, SeriesTitle from Series";
constexpr auto GENRES_QUERY = "select GenreCode, GenreAlias, ParentCode from Genres";
constexpr auto GROUPS_QUERY = "select GroupID, Title from Groups_User";
constexpr auto ARCHIVES_QUERY = "select distinct Folder from Books";
constexpr auto SEARCH_QUERY = "select SearchID, Title from Searches_User";

constexpr auto WHERE_AUTHOR = "where a.AuthorID = :id";
constexpr auto WHERE_SERIES = "where b.SeriesID = :id";
constexpr auto WHERE_GENRE = "where g.GenreCode = :id";
constexpr auto WHERE_ARCHIVE = "where b.Folder  = :id";
constexpr auto JOIN_GROUPS = "join Groups_List_User grl on grl.BookID = b.BookID and grl.GroupID = :id";
constexpr auto JOIN_SEARCHES = "join Searches_User su on su.SearchID = :id and b.SearchTitle like '%'||MHL_UPPER(su.Title)||'%'";

QString CreateAuthorTitle(const DB::IQuery & query, const int * index)
{
	QString title = query.Get<const char *>(index[2]);
	AppendTitle(title, query.Get<const char *>(index[1]));
	AppendTitle(title, query.Get<const char *>(index[3]));

	if (title.isEmpty())
		title = Loc::Tr(Loc::Ctx::ERROR, Loc::AUTHOR_NOT_SPECIFIED);

	return title;
}

IDataItem::Ptr CreateAuthorItem(const DB::IQuery & query, const int * index)
{
	auto item = NavigationItem::Create();

	item->SetId(QString::number(query.Get<long long>(index[0])));
	item->SetData(CreateAuthorTitle(query, index));

	return item;
}

int BindInt(DB::IQuery & query, const QString & id)
{
	return query.Bind(":id", id.toInt());
}

int BindString(DB::IQuery & query, const QString & id)
{
	return query.Bind(":id", id.toStdString());
}

void RequestNavigationSimpleList(NavigationMode navigationMode
	, INavigationQueryExecutor::Callback callback
	, const DatabaseUser& databaseUser
	, const QueryDescription & queryDescription
)
{
	auto db = databaseUser.Database();
	if (!db)
		return;

	databaseUser.Execute({ "Get navigation", [&, mode = navigationMode, callback = std::move(callback), db = std::move(db)] () mutable
	{
		std::unordered_map<QString, IDataItem::Ptr> cache;

		const auto query = db->CreateQuery(queryDescription.query);
		for (query->Execute(); !query->Eof(); query->Next())
		{
			auto item = queryDescription.queryInfo.extractor(*query, queryDescription.queryInfo.index);
			cache.emplace(item->GetId(), item);
		}

		IDataItem::Items items;
		items.reserve(std::size(cache));
		std::ranges::copy(cache | std::views::values, std::back_inserter(items));

		std::ranges::sort(items, [] (const IDataItem::Ptr & lhs, const IDataItem::Ptr & rhs)
		{
			return QStringWrapper { lhs->GetData() } < QStringWrapper { rhs->GetData() };
		});

		auto root = NavigationItem::Create();
		root->SetChildren(std::move(items));

		return [&, mode, callback = std::move(callback), root = std::move(root)] (size_t) mutable
		{
			callback(mode, std::move(root));
		};
	} }, 1);
}

void RequestNavigationGenres(NavigationMode navigationMode
	, INavigationQueryExecutor::Callback callback
	, const DatabaseUser & databaseUser
	, const QueryDescription & queryDescription
)
{
	auto db = databaseUser.Database();
	if (!db)
		return;

	databaseUser.Execute({"Get navigation", [&, mode = navigationMode, callback = std::move(callback), db = std::move(db)] () mutable
	{
		std::unordered_map<QString, IDataItem::Ptr> cache;

		auto root = NavigationItem::Create();
		std::unordered_multimap<QString, IDataItem::Ptr> items;
		cache.emplace("0", root);

		const auto query = db->CreateQuery(queryDescription.query);
		for (query->Execute(); !query->Eof(); query->Next())
		{
			auto item = items.emplace(query->Get<const char *>(2), queryDescription.queryInfo.extractor(*query, queryDescription.queryInfo.index))->second;
			cache.emplace(item->GetId(), std::move(item));
		}

		std::stack<QString> stack { {"0"} };

		while (!stack.empty())
		{
			auto parentId = std::move(stack.top());
			stack.pop();

			const auto parent = [&]
			{
				const auto it = cache.find(parentId);
				assert(it != cache.end());
				return it->second;
			}();

			for (auto && [it, end] = items.equal_range(parentId); it != end; ++it)
			{
				const auto & item = parent->AppendChild(std::move(it->second));
				stack.push(item->GetId());
			}
		}

		assert(std::ranges::all_of(items | std::views::values, [] (const auto & item) { return !item; }));

		return [&, mode, callback = std::move(callback), root = std::move(root), cache = std::move(cache)] (size_t) mutable
		{
			callback(mode, std::move(root));
		};
	}}, 1);
}

using NavigationRequest = void (*)(NavigationMode navigationMode
	, INavigationQueryExecutor::Callback callback
	, const DatabaseUser & databaseUser
	, const QueryDescription & queryDescription
	);


constexpr int NAVIGATION_QUERY_INDEX_AUTHOR[] { 0, 1, 2, 3 };
constexpr int NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM[] { 0, 1 };
constexpr int NAVIGATION_QUERY_INDEX_ID_ONLY_ITEM[] { 0, 0 };

constexpr QueryInfo QUERY_INFO_AUTHOR { &CreateAuthorItem, NAVIGATION_QUERY_INDEX_AUTHOR };
constexpr QueryInfo QUERY_INFO_SIMPLE_LIST_ITEM { &DatabaseUser::CreateSimpleListItem, NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM };
constexpr QueryInfo QUERY_INFO_ID_ONLY_ITEM { &DatabaseUser::CreateSimpleListItem, NAVIGATION_QUERY_INDEX_ID_ONLY_ITEM };

constexpr int MAPPING_FULL[] { BookItem::Column::Author, BookItem::Column::Title, BookItem::Column::Series, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_AUTHORS[] { BookItem::Column::Title, BookItem::Column::Series, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_SERIES[] { BookItem::Column::Author, BookItem::Column::Title, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_GENRES[] { BookItem::Column::Author, BookItem::Column::Title, BookItem::Column::Series, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };

constexpr int MAPPING_TREE_COMMON[] { BookItem::Column::Title, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_TREE_GENRES[] { BookItem::Column::Title, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };

constexpr std::pair<NavigationMode, std::pair<NavigationRequest, QueryDescription>> QUERIES[]
{
	{ NavigationMode::Authors , { &RequestNavigationSimpleList, { AUTHORS_QUERY , QUERY_INFO_AUTHOR          , WHERE_AUTHOR , nullptr      , &BindInt   , &IBooksTreeCreator::CreateAuthorsTree, BookItem::Mapping(MAPPING_AUTHORS), BookItem::Mapping(MAPPING_TREE_COMMON)}}},
	{ NavigationMode::Series  , { &RequestNavigationSimpleList, { SERIES_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, WHERE_SERIES , nullptr      , &BindInt   , &IBooksTreeCreator::CreateSeriesTree , BookItem::Mapping(MAPPING_SERIES) , BookItem::Mapping(MAPPING_TREE_COMMON)}}},
	{ NavigationMode::Genres  , { &RequestNavigationGenres    , { GENRES_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, WHERE_GENRE  , nullptr      , &BindString, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_GENRES) , BookItem::Mapping(MAPPING_TREE_GENRES)}}},
	{ NavigationMode::Groups  , { &RequestNavigationSimpleList, { GROUPS_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, nullptr      , JOIN_GROUPS  , &BindInt   , &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL)   , BookItem::Mapping(MAPPING_TREE_COMMON)}}},
	{ NavigationMode::Archives, { &RequestNavigationSimpleList, { ARCHIVES_QUERY, QUERY_INFO_ID_ONLY_ITEM    , WHERE_ARCHIVE, nullptr      , &BindString, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL)   , BookItem::Mapping(MAPPING_TREE_COMMON)}}},
	{ NavigationMode::Search  , { &RequestNavigationSimpleList, { SEARCH_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, nullptr      , JOIN_SEARCHES, &BindInt   , &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL)   , BookItem::Mapping(MAPPING_TREE_COMMON)}}},
};

static_assert(static_cast<size_t>(NavigationMode::Last) == std::size(QUERIES));

}

NavigationQueryExecutor::NavigationQueryExecutor(std::shared_ptr<DatabaseUser> databaseUser)
	: m_databaseUser(std::move(databaseUser))
{
	PLOGD << "NavigationQueryExecutor created";
}

NavigationQueryExecutor::~NavigationQueryExecutor()
{
	PLOGD << "NavigationQueryExecutor destroyed";
}

void NavigationQueryExecutor::RequestNavigation(const NavigationMode navigationMode, Callback callback) const
{
	const auto & [request, description] = FindSecond(QUERIES, navigationMode);
	std::invoke(request, navigationMode, std::move(callback), std::cref(*m_databaseUser), std::cref(description));
}

const QueryDescription & NavigationQueryExecutor::GetQueryDescription(const NavigationMode navigationMode) const
{
	const auto & [_, description] = FindSecond(QUERIES, navigationMode);
	return description;
}
