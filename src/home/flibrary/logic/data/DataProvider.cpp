#include "DataProvider.h"

#include <stack>
#include <unordered_map>

#include <plog/Log.h>
#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "util/executor/factory.h"
#include "util/IExecutor.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/logic/ILogicFactory.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr const char * BOOKS_COLUMN_NAMES[] {
#define		BOOKS_COLUMN_ITEM(NAME) #NAME,
			BOOKS_COLUMN_ITEMS_X_MACRO
#undef		BOOKS_COLUMN_ITEM
};

constexpr auto AUTHORS_QUERY = "select AuthorID, FirstName, LastName, MiddleName from Authors";
constexpr auto SERIES_QUERY = "select SeriesID, SeriesTitle from Series";
constexpr auto GENRES_QUERY = "select GenreCode, GenreAlias, ParentCode from Genres";
constexpr auto GROUPS_QUERY = "select GroupID, Title from Groups_User";
constexpr auto ARCHIVES_QUERY = "select distinct Folder from Books";

constexpr auto BOOKS_QUERY =
"select b.BookID, b.Title, coalesce(b.SeqNumber, -1), b.UpdateDate, b.LibRate, b.Lang, b.Folder, b.FileName || b.Ext, b.BookSize, coalesce(bu.IsDeleted, b.IsDeleted, 0) "
", a.AuthorID, a.LastName, a.FirstName, a.MiddleName "
", g.GenreCode, g.GenreAlias "
", coalesce(b.SeriesID, -1), s.SeriesTitle "
"from Books b "
"join Author_List al on al.BookID = b.BookID "
"join Authors a on a.AuthorID = al.AuthorID "
"join Genre_List gl on gl.BookID = b.BookID "
"join Genres g on g.GenreCode = gl.GenreCode "
"%1 "
"left join Series s on s.SeriesID = b.SeriesID "
"left join Books_User bu on bu.BookID = b.BookID "
"%2"
;

struct Book
{
	enum Value
	{
		BookId = 0,
		BookTitle,
		SeqNumber,
		UpdateDate,
		LibRate,
		Lang,
		Folder,
		FileName,
		Size,
		IsDeleted,
		AuthorId,
		AuthorLastName,
		AuthorFirstName,
		AuthorMiddleName,
		GenreCode,
		GenreTitle,
		SeriesId,
		SeriesTitle,
	};
};

constexpr std::pair<int, int> BOOK_QUERY_TO_DATA[]
{
	{ Book::BookTitle, BookItem::Column::Title },
	{ Book::SeriesTitle, BookItem::Column::Series },
	{ Book::SeqNumber, BookItem::Column::SeqNumber },
	{ Book::Folder, BookItem::Column::Folder },
	{ Book::FileName, BookItem::Column::FileName },
	{ Book::Size, BookItem::Column::Size },
	{ Book::LibRate, BookItem::Column::LibRate },
	{ Book::UpdateDate, BookItem::Column::UpdateDate },
	{ Book::Lang, BookItem::Column::Lang },
};

constexpr std::pair<int, int> BOOK_QUERY_TO_AUTHOR[]
{
	{ Book::AuthorFirstName, AuthorItem::Column::FirstName },
	{ Book::AuthorLastName, AuthorItem::Column::LastName },
	{ Book::AuthorMiddleName, AuthorItem::Column::MiddleName },
};

constexpr auto WHERE_AUTHOR = "where a.AuthorID = :id";
constexpr auto WHERE_SERIES = "where b.SeriesID = :id";
constexpr auto WHERE_GENRE = "where g.GenreCode = :id";
constexpr auto WHERE_ARCHIVE = "where b.Folder  = :id";
constexpr auto JOIN_GROUPS = "join Groups_List_User grl on grl.BookID = b.BookID and grl.GroupID = :id";

using Binder = int(*)(DB::IQuery &, const QString &);
int BindInt(DB::IQuery & query, const QString & id)
{
	return query.Bind(":id", id.toInt());
}

int BindString(DB::IQuery & query, const QString & id)
{
	return query.Bind(":id", id.toStdString());
}

using QueryDataExtractor = DataItem::Ptr(*)(const DB::IQuery & query, const int * index);

struct QueryInfo
{
	QueryDataExtractor extractor;
	const int * index;
};

class IBooksTreeCreator // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IBooksTreeCreator() = default;
	virtual DataItem::Ptr CreateAuthorsTree() const = 0;
};

using BooksTreeCreator = DataItem::Ptr(IBooksTreeCreator::*)() const;

struct QueryDescription
{
	const char * query;
	const QueryInfo & queryInfo;
	const char * whereClause;
	const char * joinClause;
	Binder binder;
	BooksTreeCreator treeCreator;
};

class INavigationQueryExecutor // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~INavigationQueryExecutor() = default;
	virtual void RequestNavigationSimpleList(const QueryDescription & queryDescription) const = 0;
	virtual void RequestNavigationGenres(const QueryDescription & queryDescription) const = 0;
};

using QueryExecutorFunctor = void(INavigationQueryExecutor::*)(const QueryDescription&) const;

class IBooksRootGenerator // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IBooksRootGenerator() = default;
	virtual DataItem::Ptr GetList(BooksTreeCreator treeCreator) const = 0;
	virtual DataItem::Ptr GetTree(BooksTreeCreator treeCreator) const = 0;
};

using BooksRootGenerator = DataItem::Ptr(IBooksRootGenerator::*)(BooksTreeCreator) const;

DataItem::Ptr CreateSimpleListItem(const DB::IQuery & query, const int * index)
{
	auto item = DataItem::Ptr(NavigationItem::Create());

	item->SetId(query.Get<const char *>(index[0]));
	item->SetData(query.Get<const char *>(index[1]));

	return item;
}

void AppendTitle(QString & title, const QString & str, const QString & delimiter = " ")
{
	if (title.isEmpty())
	{
		title = str;
		return;
	}

	if (!str.isEmpty())
		title.append(delimiter).append(str);
}

QString CreateAuthorTitle(const DB::IQuery & query, const int * index)
{
	QString title = query.Get<const char *>(index[2]);
	AppendTitle(title, query.Get<const char *>(index[1]));
	AppendTitle(title, query.Get<const char *>(index[3]));

	if (title.isEmpty())
		title = QCoreApplication::translate(Constant::Localization::CONTEXT_ERROR, Constant::Localization::AUTHOR_NOT_SPECIFIED);

	return title;
}

DataItem::Ptr CreateAuthorItem(const DB::IQuery & query, const int * index)
{
	auto item = NavigationItem::Create();

	item->SetId(QString::number(query.Get<long long>(index[0])));
	item->SetData(CreateAuthorTitle(query, index));

	return item;
}

DataItem::Ptr CreateFullAuthorItem(const DB::IQuery & query, const int * /*index*/)
{
	auto item = AuthorItem::Create();

	item->SetId(QString::number(query.Get<long long>(Book::AuthorId)));
	for (const auto & [queryIndex, dataIndex] : BOOK_QUERY_TO_AUTHOR)
		item->SetData(query.Get<const char *>(queryIndex), dataIndex);

	return item;
}

DataItem::Ptr CreateBookItem(const DB::IQuery & query)
{
	auto item = BookItem::Create();

	item->SetId(QString::number(query.Get<long long>(Book::BookId)));
	for (const auto & [queryIndex, dataIndex] : BOOK_QUERY_TO_DATA)
		item->SetData(query.Get<const char *>(queryIndex), dataIndex);

	auto & typed = *item->To<BookItem>();
	typed.deleted = query.Get<int>(Book::IsDeleted);

	return item;
}

template<typename KeyType, typename BindType = KeyType>
std::optional<KeyType> UpdateDictionary(std::unordered_map<KeyType, DataItem::Ptr> & dictionary
	, const DB::IQuery & query
	, const QueryInfo & queryInfo
	, const std::function<bool(const DataItem &)> & filter = [] (const DataItem &) { return true; }
)
{
	auto key = query.Get<BindType>(queryInfo.index[0]);
	const auto it = dictionary.find(key);
	if (it != dictionary.end())
		return it->first;

	auto item = queryInfo.extractor(query, queryInfo.index);
	return filter(*item) ? std::optional<KeyType>(dictionary.emplace(key, std::move(item)).first->first) : std::nullopt;
}

template<typename KeyType>
void Add(std::unordered_set<KeyType> & set, std::optional<KeyType> key)
{
	if (key)
		set.emplace(std::move(*key));
}

QString GetTitle(const DataItem::Ptr & item)
{
	return item->GetData();
}

QString GetAuthorShortName(const DataItem::Ptr & item)
{
	QString last = item->GetData(AuthorItem::Column::LastName);
	QString first = item->GetData(AuthorItem::Column::FirstName);
	QString middle = item->GetData(AuthorItem::Column::MiddleName);

	for (int i = 0; i < 2; ++i)
	{
		if (!last.isEmpty())
			break;

		last = std::move(first);
		first = std::move(middle);
		middle = QString();
	}

	if (last.isEmpty())
		last = QCoreApplication::translate(Constant::Localization::CONTEXT_ERROR, Constant::Localization::AUTHOR_NOT_SPECIFIED);

	auto name = last;

	const auto append = [&] (const QString & str)
	{
		if (str.isEmpty())
			return;

		AppendTitle(name, str.first(1) + ".");
	};

	append(first);
	append(middle);

	return name;
}


constexpr int NAVIGATION_QUERY_INDEX_AUTHOR[] { 0, 1, 2, 3 };
constexpr int NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM[] { 0, 1 };
constexpr int NAVIGATION_QUERY_INDEX_ID_ONLY_ITEM[] { 0, 0 };

constexpr int BOOKS_QUERY_INDEX_SERIES[] { Book::SeriesId, Book::SeriesTitle };
constexpr int BOOKS_QUERY_INDEX_AUTHOR[] { Book::AuthorId };
constexpr int BOOKS_QUERY_INDEX_GENRE[] { Book::GenreCode, Book::GenreTitle };

constexpr QueryInfo QUERY_INFO_AUTHOR { &CreateAuthorItem, NAVIGATION_QUERY_INDEX_AUTHOR };
constexpr QueryInfo QUERY_INFO_SIMPLE_LIST_ITEM { &CreateSimpleListItem, NAVIGATION_QUERY_INDEX_SIMPLE_LIST_ITEM };
constexpr QueryInfo QUERY_INFO_ID_ONLY_ITEM { &CreateSimpleListItem, NAVIGATION_QUERY_INDEX_ID_ONLY_ITEM };

constexpr std::pair<NavigationMode, std::pair<QueryExecutorFunctor, QueryDescription>> QUERIES[]
{
	{ NavigationMode::Authors , { &INavigationQueryExecutor::RequestNavigationSimpleList, { AUTHORS_QUERY , QUERY_INFO_AUTHOR          , WHERE_AUTHOR , nullptr    , &BindInt   , &IBooksTreeCreator::CreateAuthorsTree}}},
	{ NavigationMode::Series  , { &INavigationQueryExecutor::RequestNavigationSimpleList, { SERIES_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, WHERE_SERIES , nullptr    , &BindInt   }}},
	{ NavigationMode::Genres  , { &INavigationQueryExecutor::RequestNavigationGenres    , { GENRES_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, WHERE_GENRE  , nullptr    , &BindString}}},
	{ NavigationMode::Groups  , { &INavigationQueryExecutor::RequestNavigationSimpleList, { GROUPS_QUERY  , QUERY_INFO_SIMPLE_LIST_ITEM, nullptr      , JOIN_GROUPS, &BindInt   }}},
	{ NavigationMode::Archives, { &INavigationQueryExecutor::RequestNavigationSimpleList, { ARCHIVES_QUERY, QUERY_INFO_ID_ONLY_ITEM    , WHERE_ARCHIVE, nullptr    , &BindString}}},
};

constexpr std::pair<ViewMode, BooksRootGenerator> BOOKS_GENERATORS[]
{
	{ ViewMode::List, &IBooksRootGenerator::GetList },
	{ ViewMode::Tree, &IBooksRootGenerator::GetTree },
};

struct QStringWrapper
{
	const QString & data;
	bool operator<(const QStringWrapper & rhs) const
	{
		return QString::compare(data, rhs.data, Qt::CaseInsensitive) < 0;
	}
};

class BooksGenerator final
	: public IBooksRootGenerator
	, public IBooksTreeCreator
{
public:
	BooksGenerator(DB::IDatabase & db, const QString & navigationId, const QueryDescription & description)
	{
		const auto query = db.CreateQuery(QString(BOOKS_QUERY).arg(description.joinClause, description.whereClause).toStdString());
		[[maybe_unused]] const auto result = description.binder(*query, navigationId);
		assert(result == 0);
		for (query->Execute(); !query->Eof(); query->Next())
		{
			auto & book = m_books[query->Get<int>(Book::BookId)];
			std::get<1>(book) = UpdateDictionary<int>(m_series, *query, QueryInfo(&CreateSimpleListItem, BOOKS_QUERY_INDEX_SERIES), [](const DataItem & item){ return item.GetId() != "-1"; });
			Add(std::get<2>(book), UpdateDictionary<int>(m_authors, *query, QueryInfo(&CreateFullAuthorItem, BOOKS_QUERY_INDEX_AUTHOR)));
			Add(std::get<3>(book), UpdateDictionary<QString, const char *>(m_genres, *query, QueryInfo(&CreateSimpleListItem, BOOKS_QUERY_INDEX_GENRE), [] (const DataItem & item)
			{
				return !(item.GetData().isEmpty() || item.GetData()[0].isDigit());
			}));

			if (!std::get<0>(book))
				std::get<0>(book) = CreateBookItem(*query);
		}

		const auto toAuthorItemComparable = [] (const DataItem::Ptr & author)
		{
			return std::make_tuple(
				  QStringWrapper { author->GetData(AuthorItem::Column::LastName) }
				, QStringWrapper { author->GetData(AuthorItem::Column::FirstName) }
				, QStringWrapper { author->GetData(AuthorItem::Column::MiddleName) }
			);
		};

		const auto authorsComparator = [&] (const DataItem::Ptr & lhs, const DataItem::Ptr & rhs)
		{
			return toAuthorItemComparable(lhs) < toAuthorItemComparable(rhs);
		};

		const auto genresComparator = [] (const DataItem::Ptr & lhs, const DataItem::Ptr & rhs)
		{
			return QStringWrapper { lhs->GetId() } < QStringWrapper { rhs->GetId() };
		};

		for (auto & [book, seriesId, authorIds, genreIds] : m_books | std::views::values)
		{
			book->SetData(Join(m_authors, authorIds, &GetAuthorShortName, authorsComparator), BookItem::Column::Author);
			book->SetData(Join(m_genres, genreIds, &GetTitle, genresComparator), BookItem::Column::Genre);
		}
	}

private: // IBooksRootGenerator
	[[nodiscard]] DataItem::Ptr GetList(BooksTreeCreator) const override
	{
		DataItem::Items items;
		items.reserve(std::size(m_books));
		std::ranges::transform(m_books | std::views::values, std::back_inserter(items), [] (const auto & item)
		{
			return std::get<0>(item);
		});

		DataItem::Ptr root(NavigationItem::Create());
		root->SetChildren(std::move(items));
		return root;
	}

	[[nodiscard]] DataItem::Ptr GetTree(const BooksTreeCreator creator) const override
	{
		return ((*this).*creator)();
	}

private: // IBooksTreeCreator
	[[nodiscard]] DataItem::Ptr CreateAuthorsTree() const override
	{
		DataItem::Ptr root(BookItem::Create());
		std::ranges::for_each(BOOKS_COLUMN_NAMES, [&, n = 0] (const auto * columnName) mutable
		{
			root->SetData(columnName, n++);
		});
			
		for (const auto & series : m_series | std::views::values)
			root->AppendChild(series);

		for (const auto & [book, seriesId, authorIds, genreIds] : m_books | std::views::values)
		{
			if (!seriesId)
			{
				root->AppendChild(book);
				continue;
			}

			const auto it = m_series.find(*seriesId);
			assert(it != m_series.end());
			it->second->AppendChild(book);
		}

		return root;
	}

private:
	template <typename T>
	static QString Join(const std::unordered_map<T, DataItem::Ptr> & dictionary
		, const std::unordered_set<T> & keyIds
		, const std::function<QString(const DataItem::Ptr&)> & toString
		, const std::function<bool(const DataItem::Ptr & lhs, const DataItem::Ptr & rhs)> & comparator
	)
	{
		DataItem::Items values;
		values.reserve(std::size(keyIds));
		std::ranges::transform(keyIds, std::back_inserter(values), [&] (const T & id)
		{
			const auto it = dictionary.find(id);
			assert(it != dictionary.end());
			return it->second;
		});

		std::ranges::sort(values, comparator);

		QString result;
		for (const auto & value : values)
			AppendTitle(result, toString(value), ", ");

		return result;
	}

private:
	std::unordered_map<int, std::tuple<DataItem::Ptr, std::optional<int>, std::unordered_set<int>, std::unordered_set<QString>>> m_books;
	std::unordered_map<int, DataItem::Ptr> m_series;
	std::unordered_map<int, DataItem::Ptr> m_authors;
	std::unordered_map<QString, DataItem::Ptr> m_genres;
};

}

class DataProvider::Impl
	: virtual public INavigationQueryExecutor
{
public:
	explicit Impl(std::shared_ptr<ILogicFactory> logicFactory)
		: m_logicFactory(std::move(logicFactory))
	{
	}

	void SetNavigationMode(const NavigationMode navigationMode)
	{
		m_navigationMode = navigationMode;
	}

	void SetNavigationViewMode(const ViewMode viewMode)
	{
		m_navigationViewMode = viewMode;
	}

	void SetNavigationId(QString id)
	{
		m_navigationId = std::move(id);
	}

	void SetBooksViewMode(const ViewMode viewMode)
	{
		m_booksViewMode = viewMode;
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
		const auto [functor, description] = FindSecond(QUERIES, m_navigationMode);
		const auto & booksGenerator = FindSecond(BOOKS_GENERATORS, m_booksViewMode);

		(*m_executor)({ "Get books", [&, navigationId = m_navigationId] () mutable
		{
			const BooksGenerator generator(*m_db, navigationId, description);
			auto root = (generator.*booksGenerator)(description.treeCreator);
			return [&, navigationId = std::move(navigationId), root = std::move(root)] (size_t) mutable
			{
				SendBooksCallback(navigationId, std::move(root));
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

			DataItem::Ptr root(NavigationItem::Create());
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

			DataItem::Ptr root(NavigationItem::Create());
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
	std::unique_ptr<Util::IExecutor> CreateExecutor()
	{
		const auto createDatabase = [this]
		{
			m_db = m_logicFactory->GetDatabase();
//			m_db->RegisterObserver(this);
		};

		const auto destroyDatabase = [this]
		{
//			m_db->UnregisterObserver(this);
			m_db.reset();
		};

		return m_logicFactory->GetExecutor({
			  [=] { createDatabase(); }
			, [ ] { QGuiApplication::setOverrideCursor(Qt::BusyCursor); }
			, [ ] { QGuiApplication::restoreOverrideCursor(); }
			, [=] { destroyDatabase(); }
		});
	}

	void SendNavigationCallback(const NavigationMode mode, DataItem::Ptr root) const
	{
		if (mode == m_navigationMode)
			m_navigationRequestCallback(std::move(root));
	}

	void SendBooksCallback(const QString & id, DataItem::Ptr root) const
	{
		if (id == m_navigationId)
			m_booksRequestCallback(std::move(root));
	}

private:
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	std::unique_ptr<DB::IDatabase> m_db;
	std::unique_ptr<Util::IExecutor> m_executor{ CreateExecutor() };
	NavigationMode m_navigationMode { NavigationMode::Unknown };
	ViewMode m_navigationViewMode { ViewMode::Unknown };
	ViewMode m_booksViewMode { ViewMode::Unknown };
	QString m_navigationId;
	Callback m_navigationRequestCallback;
	Callback m_booksRequestCallback;
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

void DataProvider::SetNavigationViewMode(const ViewMode viewMode)
{
	m_impl->SetNavigationViewMode(viewMode);
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

void DataProvider::RequestNavigation() const
{
	m_impl->RequestNavigation();
}

void DataProvider::RequestBooks() const
{
	m_impl->RequestBooks();
}
