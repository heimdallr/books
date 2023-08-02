#include "DataProvider.h"

#include <stack>
#include <unordered_map>

#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/logic/ILogicFactory.h"

#include "shared/DatabaseUser.h"

#include "BooksTreeGenerator.h"
#include "DataItem.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto AUTHORS_QUERY = "select AuthorID, FirstName, LastName, MiddleName from Authors";
constexpr auto SERIES_QUERY = "select SeriesID, SeriesTitle from Series";
constexpr auto GENRES_QUERY = "select GenreCode, GenreAlias, ParentCode from Genres";
constexpr auto GROUPS_QUERY = "select GroupID, Title from Groups_User";
constexpr auto ARCHIVES_QUERY = "select distinct Folder from Books";

constexpr auto WHERE_AUTHOR = "where a.AuthorID = :id";
constexpr auto WHERE_SERIES = "where b.SeriesID = :id";
constexpr auto WHERE_GENRE = "where g.GenreCode = :id";
constexpr auto WHERE_ARCHIVE = "where b.Folder  = :id";
constexpr auto JOIN_GROUPS = "join Groups_List_User grl on grl.BookID = b.BookID and grl.GroupID = :id";

int BindInt(DB::IQuery & query, const QString & id)
{
	return query.Bind(":id", id.toInt());
}

int BindString(DB::IQuery & query, const QString & id)
{
	return query.Bind(":id", id.toStdString());
}

class INavigationQueryExecutor // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~INavigationQueryExecutor() = default;
	virtual void RequestNavigationSimpleList(const QueryDescription & queryDescription) const = 0;
	virtual void RequestNavigationGenres(const QueryDescription & queryDescription) const = 0;
};

using QueryExecutorFunctor = void(INavigationQueryExecutor::*)(const QueryDescription&) const;

QString CreateAuthorTitle(const DB::IQuery & query, const int * index)
{
	QString title = query.Get<const char *>(index[2]);
	AppendTitle(title, query.Get<const char *>(index[1]));
	AppendTitle(title, query.Get<const char *>(index[3]));

	if (title.isEmpty())
		title = Loc::Tr(Loc::Ctx::ERROR, Loc::AUTHOR_NOT_SPECIFIED);

	return title;
}

DataItem::Ptr CreateAuthorItem(const DB::IQuery & query, const int * index)
{
	auto item = NavigationItem::Create();

	item->SetId(QString::number(query.Get<long long>(index[0])));
	item->SetData(CreateAuthorTitle(query, index));

	return item;
}

constexpr int NAVIGATION_QUERY_INDEX_AUTHOR[] { 0, 1, 2, 3 };
constexpr int NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM[] { 0, 1 };
constexpr int NAVIGATION_QUERY_INDEX_ID_ONLY_ITEM[] { 0, 0 };

constexpr QueryInfo QUERY_INFO_AUTHOR { &CreateAuthorItem, NAVIGATION_QUERY_INDEX_AUTHOR };
constexpr QueryInfo QUERY_INFO_SIMPLE_LIST_ITEM { &CreateSimpleListItem, NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM };
constexpr QueryInfo QUERY_INFO_ID_ONLY_ITEM { &CreateSimpleListItem, NAVIGATION_QUERY_INDEX_ID_ONLY_ITEM };

constexpr int MAPPING_FULL[] { BookItem::Column::Author, BookItem::Column::Title, BookItem::Column::Series, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_AUTHORS[] { BookItem::Column::Title, BookItem::Column::Series, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_SERIES[] { BookItem::Column::Author, BookItem::Column::Title, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_GENRES[] { BookItem::Column::Author, BookItem::Column::Title, BookItem::Column::Series, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };

constexpr int MAPPING_TREE_COMMON[] { BookItem::Column::Title, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Genre, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };
constexpr int MAPPING_TREE_GENRES[] { BookItem::Column::Title, BookItem::Column::SeqNumber, BookItem::Column::Size, BookItem::Column::Folder, BookItem::Column::FileName, BookItem::Column::LibRate, BookItem::Column::UpdateDate, BookItem::Column::Lang };

constexpr std::pair<NavigationMode, std::pair<QueryExecutorFunctor, QueryDescription>> QUERIES[]
{
	{ NavigationMode::Authors , { &INavigationQueryExecutor::RequestNavigationSimpleList, { AUTHORS_QUERY , QUERY_INFO_AUTHOR          , WHERE_AUTHOR , nullptr    , &BindInt   , &IBooksTreeCreator::CreateAuthorsTree, BookItem::Mapping(MAPPING_AUTHORS), BookItem::Mapping(MAPPING_TREE_COMMON)}}},
	{ NavigationMode::Series  , { &INavigationQueryExecutor::RequestNavigationSimpleList, { SERIES_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, WHERE_SERIES , nullptr    , &BindInt   , &IBooksTreeCreator::CreateSeriesTree , BookItem::Mapping(MAPPING_SERIES) , BookItem::Mapping(MAPPING_TREE_COMMON)}}},
	{ NavigationMode::Genres  , { &INavigationQueryExecutor::RequestNavigationGenres    , { GENRES_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, WHERE_GENRE  , nullptr    , &BindString, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_GENRES) , BookItem::Mapping(MAPPING_TREE_GENRES)}}},
	{ NavigationMode::Groups  , { &INavigationQueryExecutor::RequestNavigationSimpleList, { GROUPS_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, nullptr      , JOIN_GROUPS, &BindInt   , &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL)   , BookItem::Mapping(MAPPING_TREE_COMMON)}}},
	{ NavigationMode::Archives, { &INavigationQueryExecutor::RequestNavigationSimpleList, { ARCHIVES_QUERY, QUERY_INFO_ID_ONLY_ITEM    , WHERE_ARCHIVE, nullptr    , &BindString, &IBooksTreeCreator::CreateGeneralTree, BookItem::Mapping(MAPPING_FULL)   , BookItem::Mapping(MAPPING_TREE_COMMON)}}},
};

struct BooksViewModeDescription
{
	IBooksRootGenerator::Generator generator;
	QueryDescription::MappingGetter mapping;
};

constexpr std::pair<ViewMode, BooksViewModeDescription> BOOKS_GENERATORS[]
{
	{ ViewMode::List, { &IBooksRootGenerator::GetList, &QueryDescription::GetListMapping } },
	{ ViewMode::Tree, { &IBooksRootGenerator::GetTree, &QueryDescription::GetTreeMapping } },
};

}

class DataProvider::Impl
	: virtual public INavigationQueryExecutor
	, DatabaseUser
{
public:
	explicit Impl(std::shared_ptr<ILogicFactory> logicFactory)
		: DatabaseUser(std::move(logicFactory))
	{
	}

	void SetNavigationMode(const NavigationMode navigationMode)
	{
		m_navigationMode = navigationMode;
		m_navigationTimer->start();
	}

	void SetNavigationId(QString id)
	{
		m_navigationId = std::move(id);
		m_booksTimer->start();
	}

	void SetBooksViewMode(const ViewMode viewMode)
	{
		m_booksViewMode = viewMode;
		m_booksTimer->start();
	}

	void SetNavigationRequestCallback(Callback callback)
	{
		m_navigationRequestCallback = std::move(callback);
	}

	void SetBookRequestCallback(Callback callback)
	{
		m_booksRequestCallback = std::move(callback);
	}

	void RequestNavigation() const
	{
		const auto & [invoker, description] = FindSecond(QUERIES, m_navigationMode);
		std::invoke(invoker, this, std::cref(description));
	}

	void RequestBooks() const
	{
		if (m_navigationId.isEmpty() || m_booksViewMode == ViewMode::Unknown)
			return;

		const auto booksGeneratorReady = m_booksGenerator
			&& m_booksGenerator->GetNavigationMode() == m_navigationMode
			&& m_booksGenerator->GetNavigationId() == m_navigationId
			;

		const auto & [_, description] = FindSecond(QUERIES, m_navigationMode);
		const auto & [booksGenerator, columnMapper] = FindSecond(BOOKS_GENERATORS, m_booksViewMode);

		if (booksGeneratorReady && m_booksGenerator->GetBooksViewMode() == m_booksViewMode)
			return SendBooksCallback(m_navigationId, m_booksGenerator->GetCached(), (description.*columnMapper)());

		(*m_executor)({ "Get books", [&
			, navigationMode = m_navigationMode
			, navigationId = m_navigationId
			, viewMode = m_booksViewMode
			, generator = std::move(m_booksGenerator)
			, booksGeneratorReady
		] () mutable
		{
			if (!booksGeneratorReady)
				generator = std::make_unique<BooksTreeGenerator>(*m_db, navigationMode, navigationId, description);

			generator->SetBooksViewMode(viewMode);
			auto root = (*generator.*booksGenerator)(description.treeCreator);
			return [&, navigationId = std::move(navigationId), root = std::move(root), generator = std::move(generator)] (size_t) mutable
			{
				m_booksGenerator = std::move(generator);
				SendBooksCallback(navigationId, std::move(root), (description.*columnMapper)());
			};
		} }, 2);
	}

private: // INavigationQueryExecutor
	void RequestNavigationSimpleList(const QueryDescription & queryDescription) const override
	{
		(*m_executor)({ "Get navigation", [&, mode = m_navigationMode] ()
		{
			std::unordered_map<QString, DataItem::Ptr> cache;

			const auto query = m_db->CreateQuery(queryDescription.query);
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto item = queryDescription.queryInfo.extractor(*query, queryDescription.queryInfo.index);
				cache.emplace(item->GetId(), item);
			}

			DataItem::Items items;
			items.reserve(std::size(cache));
			std::ranges::copy(cache | std::views::values, std::back_inserter(items));

			std::ranges::sort(items, [] (const DataItem::Ptr & lhs, const DataItem::Ptr & rhs)
			{
				return QStringWrapper { lhs->GetData() } < QStringWrapper { rhs->GetData() };
			});

			auto root = NavigationItem::Create();
			root->SetChildren(std::move(items));

			return [&, mode, root = std::move(root)] (size_t) mutable
			{
				SendNavigationCallback(mode, std::move(root));
			};
		} }, 1);
	}

	void RequestNavigationGenres(const QueryDescription & queryDescription) const override
	{
		(*m_executor)({"Get navigation", [&, mode = m_navigationMode] ()
		{
			std::unordered_map<QString, DataItem::Ptr> cache;

			auto root = NavigationItem::Create();
			std::unordered_multimap<QString, DataItem::Ptr> items;
			cache.emplace("0", root);

			const auto query = m_db->CreateQuery(queryDescription.query);
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

			return [&, mode, root = std::move(root), cache = std::move(cache)] (size_t) mutable
			{
				SendNavigationCallback(mode, std::move(root));
			};
		}}, 1);
	}

private:
	void SendNavigationCallback(const NavigationMode mode, DataItem::Ptr root) const
	{
		if (mode == m_navigationMode)
			m_navigationRequestCallback(std::move(root));
	}

	void SendBooksCallback(const QString & id, DataItem::Ptr root, const BookItem::Mapping & columnMapping) const
	{
		if (id != m_navigationId)
			return;

		BookItem::mapping = &columnMapping;
		m_booksRequestCallback(std::move(root));
	}

private:
	NavigationMode m_navigationMode { NavigationMode::Unknown };
	ViewMode m_booksViewMode { ViewMode::Unknown };
	QString m_navigationId;
	Callback m_navigationRequestCallback;
	Callback m_booksRequestCallback;

	mutable std::shared_ptr<BooksTreeGenerator> m_booksGenerator;
	PropagateConstPtr<QTimer> m_navigationTimer { CreateTimer([&] { RequestNavigation(); }) };
	PropagateConstPtr<QTimer> m_booksTimer { CreateTimer([&] { RequestBooks(); }) };
};

DataProvider::DataProvider(std::shared_ptr<ILogicFactory> logicFactory)
	: m_impl(std::move(logicFactory))
{
	PLOGD << "DataProvider created";
}

DataProvider::~DataProvider()
{
	PLOGD << "DataProvider destroyed";
}

void DataProvider::SetNavigationMode(const NavigationMode navigationMode)
{
	m_impl->SetNavigationMode(navigationMode);
}

void DataProvider::SetNavigationId(QString id)
{
	m_impl->SetNavigationId(std::move(id));
}

void DataProvider::SetBooksViewMode(const ViewMode viewMode)
{
	m_impl->SetBooksViewMode(viewMode);
}

void DataProvider::SetNavigationRequestCallback(Callback callback)
{
	m_impl->SetNavigationRequestCallback(std::move(callback));
}

void DataProvider::SetBookRequestCallback(Callback callback)
{
	m_impl->SetBookRequestCallback(std::move(callback));
}

void DataProvider::RequestBooks() const
{
	m_impl->RequestBooks();
}
