#include "NavigationQueryExecutor.h"

#include <ranges>
#include <stack>
#include <unordered_map>

#include <QHash>

#include "fnd/FindPair.h"
#include "interface/constants/Enums.h"
#include "interface/constants/GenresLocalization.h"
#include "interface/constants/Localization.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/SortNavigation.h"
#include "database/DatabaseUtil.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "BooksTreeGenerator.h"
#include "inpx/src/util/constant.h"

#include <plog/Log.h>

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto AUTHORS_QUERY = "select AuthorID, FirstName, LastName, MiddleName from Authors";
constexpr auto SERIES_QUERY = "select SeriesID, SeriesTitle from Series";
constexpr auto KEYWORDS_QUERY = "select KeywordID, KeywordTitle from Keywords";
constexpr auto GENRES_QUERY = "select g.GenreCode, g.GenreAlias, g.FB2Code, g.ParentCode from Genres g where exists (select 42 from Genres c where c.ParentCode = g.GenreCode) or exists (select 42 from Genre_List l where l.GenreCode = g.GenreCode)";
constexpr auto GROUPS_QUERY = "select GroupID, Title from Groups_User";
constexpr auto ARCHIVES_QUERY = "select FolderID, FolderTitle from Folders";
constexpr auto SEARCH_QUERY = "select SearchID, Title from Searches_User";

constexpr auto WHERE_AUTHOR  = "where a.AuthorID  = :id";
constexpr auto WHERE_SERIES  = "where b.SeriesID  = :id";
constexpr auto WHERE_GENRE   = "where g.GenreCode = :id";
constexpr auto WHERE_ARCHIVE = "where b.FolderID  = :id";
constexpr auto JOIN_GROUPS = "join Groups_List_User grl on grl.BookID = b.BookID and grl.GroupID = :id";
constexpr auto JOIN_SEARCHES = "join Searches_User su on su.SearchID = :id and b.SearchTitle like case su.mode when 0 then '%'||su.SearchTitle||'%' when 1 then su.SearchTitle||'%' when 2 then '%'||su.SearchTitle when 3 then su.SearchTitle end";
constexpr auto JOIN_KEYWORDS = "join Keyword_List kl on kl.BookID = b.BookID and kl.KeywordID = :id";

using Cache = std::unordered_map<NavigationMode, IDataItem::Ptr>;

QString CreateAuthorTitle(const DB::IQuery & query, const size_t * index)
{
	QString title = query.Get<const char *>(index[2]);
	AppendTitle(title, query.Get<const char *>(index[1]));
	AppendTitle(title, query.Get<const char *>(index[3]));

	if (title.isEmpty())
		title = Loc::Tr(Loc::Ctx::ERROR, Loc::AUTHOR_NOT_SPECIFIED);

	return title;
}

IDataItem::Ptr CreateAuthorItem(const DB::IQuery & query, const size_t * index)
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
	, const IDatabaseUser& databaseUser
	, const QueryDescription & queryDescription
	, Cache & cache
)
{
	auto db = databaseUser.Database();
	if (!db)
		return;

	databaseUser.Execute({ "Get navigation", [&queryDescription, &cache, mode = navigationMode, callback = std::move(callback), db = std::move(db)] () mutable
	{
		std::unordered_map<QString, IDataItem::Ptr> index;

		const auto query = db->CreateQuery(queryDescription.query);
		for (query->Execute(); !query->Eof(); query->Next())
		{
			auto item = queryDescription.queryInfo.extractor(*query, queryDescription.queryInfo.index);
			index.try_emplace(item->GetId(), item);
		}

		IDataItem::Items items;
		items.reserve(std::size(index));
		std::ranges::copy(index | std::views::values, std::back_inserter(items));

		std::ranges::sort(items, [] (const IDataItem::Ptr & lhs, const IDataItem::Ptr & rhs)
		{
			return QStringWrapper { lhs->GetData() } < QStringWrapper { rhs->GetData() };
		});

		auto root = NavigationItem::Create();
		root->SetChildren(std::move(items));

		return [&cache, mode, callback = std::move(callback), root = std::move(root)] (size_t) mutable
		{
			cache[mode] = root;
			callback(mode, std::move(root));
		};
	} }, 1);
}

void RequestNavigationGenres(NavigationMode navigationMode
	, INavigationQueryExecutor::Callback callback
	, const IDatabaseUser & databaseUser
	, const QueryDescription & queryDescription
	, Cache & cache
)
{
	auto db = databaseUser.Database();
	if (!db)
		return;

	databaseUser.Execute({"Get navigation", [&queryDescription, &cache, mode = navigationMode, callback = std::move(callback), db = std::move(db)] () mutable
	{
		std::unordered_map<QString, IDataItem::Ptr> index;

		auto root = NavigationItem::Create();
		std::unordered_multimap<QString, IDataItem::Ptr> items;
		index.try_emplace("0", root);

		const auto query = db->CreateQuery(queryDescription.query);
		for (query->Execute(); !query->Eof(); query->Next())
		{
			auto item = items.emplace(query->Get<const char *>(3), queryDescription.queryInfo.extractor(*query, queryDescription.queryInfo.index))->second;
			index.try_emplace(item->GetId(), std::move(item));
		}

		{
			std::stack<QString> stack { {"0"} };

			while (!stack.empty())
			{
				auto parentId = std::move(stack.top());
				stack.pop();

				const auto parent = [&]
				{
					const auto it = index.find(parentId);
					assert(it != index.end());
					return it->second;
				}();

				for (auto && [it, end] = items.equal_range(parentId); it != end; ++it)
				{
					const auto & item = parent->AppendChild(std::move(it->second));
					stack.push(item->GetId());
				}
			}
		}

		assert(std::ranges::all_of(items | std::views::values, [] (const auto & item) { return !item; }));
		const auto rootName = Loc::Tr(GENRE, QString::fromStdWString(std::wstring(DATE_ADDED_CODE)).toStdString().data());
		if (const auto dateRoot = root->FindChild([&] (const IDataItem & item) { return item.GetData() == rootName; }))
		{
			std::stack<IDataItem *> stack { {dateRoot.get()} };
			while (!stack.empty())
			{
				auto * parent = stack.top();
				stack.pop();

				parent->SortChildren([] (const IDataItem & lhs, const IDataItem & rhs) { return lhs.GetData() < rhs.GetData(); });
				for (size_t i = 0, sz = parent->GetChildCount(); i < sz; ++i)
					stack.push(parent->GetChild(i).get());
			}
		}

		return [&cache, mode, callback = std::move(callback), root = std::move(root)] (size_t) mutable
		{
			cache[mode] = root;
			callback(mode, std::move(root));
		};
	}}, 1);
}

using NavigationRequest = void (*)(NavigationMode navigationMode
	, INavigationQueryExecutor::Callback callback
	, const IDatabaseUser & databaseUser
	, const QueryDescription & queryDescription
	, Cache & cache
	);


constexpr size_t NAVIGATION_QUERY_INDEX_AUTHOR[] { 0, 1, 2, 3 };
constexpr size_t NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM[] { 0, 1 };
constexpr size_t NAVIGATION_QUERY_INDEX_GENRE_ITEM[] { 0, 1, 2 };

constexpr QueryInfo QUERY_INFO_AUTHOR { &CreateAuthorItem, NAVIGATION_QUERY_INDEX_AUTHOR };
constexpr QueryInfo QUERY_INFO_SIMPLE_LIST_ITEM { &DatabaseUtil::CreateSimpleListItem, NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM };
constexpr QueryInfo QUERY_INFO_GENRE_ITEM { &DatabaseUtil::CreateGenreItem, NAVIGATION_QUERY_INDEX_GENRE_ITEM };

constexpr int MAPPING_FULL[] { BookItem::Column::Author, BookItem::Column::Title, BookItem::Column::Series, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_AUTHORS[] { BookItem::Column::Title, BookItem::Column::Series, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_SERIES[] { BookItem::Column::Author, BookItem::Column::Title, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_GENRES[] { BookItem::Column::Author, BookItem::Column::Title, BookItem::Column::Series, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };

constexpr int MAPPING_TREE_COMMON[] { BookItem::Column::Title, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_TREE_GENRES[] { BookItem::Column::Title, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UserRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };

constexpr std::pair<NavigationMode, std::pair<NavigationRequest, QueryDescription>> QUERIES[]
{
	{ NavigationMode::Authors , { &RequestNavigationSimpleList, { AUTHORS_QUERY , QUERY_INFO_AUTHOR          , WHERE_AUTHOR , nullptr      , &BindInt   , &IBooksTreeCreator::CreateAuthorsTree, BookItem::Mapping(MAPPING_AUTHORS), BookItem::Mapping(MAPPING_TREE_COMMON)}}},
	{ NavigationMode::Series  , { &RequestNavigationSimpleList, { SERIES_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, WHERE_SERIES , nullptr      , &BindInt   , &IBooksTreeCreator::CreateSeriesTree , BookItem::Mapping(MAPPING_SERIES) , BookItem::Mapping(MAPPING_TREE_COMMON)}}},
	{ NavigationMode::Genres  , { &RequestNavigationGenres    , { GENRES_QUERY  , QUERY_INFO_GENRE_ITEM      , WHERE_GENRE  , nullptr      , &BindString, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_GENRES) , BookItem::Mapping(MAPPING_TREE_GENRES)}}},
	{ NavigationMode::Keywords, { &RequestNavigationSimpleList, { KEYWORDS_QUERY, QUERY_INFO_SIMPLE_LIST_ITEM, nullptr      , JOIN_KEYWORDS, &BindInt   , &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL)   , BookItem::Mapping(MAPPING_TREE_COMMON)}}},
	{ NavigationMode::Groups  , { &RequestNavigationSimpleList, { GROUPS_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, nullptr      , JOIN_GROUPS  , &BindInt   , &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL)   , BookItem::Mapping(MAPPING_TREE_COMMON)}}},
	{ NavigationMode::Archives, { &RequestNavigationSimpleList, { ARCHIVES_QUERY, QUERY_INFO_SIMPLE_LIST_ITEM, WHERE_ARCHIVE, nullptr      , &BindInt   , &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL)   , BookItem::Mapping(MAPPING_TREE_COMMON)}}},
	{ NavigationMode::Search  , { &RequestNavigationSimpleList, { SEARCH_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, nullptr      , JOIN_SEARCHES, &BindInt   , &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL)   , BookItem::Mapping(MAPPING_TREE_COMMON)}}},
};

static_assert(static_cast<size_t>(NavigationMode::Last) == std::size(QUERIES));

constexpr std::pair<const char *, NavigationMode> TABLES[]
{
	{ "Authors"      , NavigationMode::Authors },
	{ "Series"       , NavigationMode::Series },
	{ "Genres"       , NavigationMode::Genres },
	{ "Keywords"     , NavigationMode::Keywords },
	{ "Groups_User"  , NavigationMode::Groups },
	{ "Books"        , NavigationMode::Archives },
	{ "Searches_User", NavigationMode::Search },
};
static_assert(static_cast<size_t>(NavigationMode::Last) == std::size(TABLES));

}

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
	void OnUpdate(std::string_view, std::string_view, int64_t) override{}
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
	PLOGD << "NavigationQueryExecutor created";
}

NavigationQueryExecutor::~NavigationQueryExecutor()
{
	PLOGD << "NavigationQueryExecutor destroyed";
}

void NavigationQueryExecutor::RequestNavigation(const NavigationMode navigationMode, Callback callback) const
{
	if (const auto it = m_impl->cache.find(navigationMode); it != m_impl->cache.end())
		return callback(navigationMode, it->second);

	const auto & [request, description] = FindSecond(QUERIES, navigationMode);
	(*request)(navigationMode, std::move(callback), *m_impl->databaseUser, description, m_impl->cache);
}

const QueryDescription & NavigationQueryExecutor::GetQueryDescription(const NavigationMode navigationMode) const
{
	const auto & [_, description] = FindSecond(QUERIES, navigationMode);
	return description;
}
