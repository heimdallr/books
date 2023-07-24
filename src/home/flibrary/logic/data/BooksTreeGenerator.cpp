#include "BooksTreeGenerator.h"

#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include <QCoreApplication>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

using IdsSet = std::unordered_set<long long>;

constexpr const char * BOOKS_COLUMN_NAMES[] {
#define		BOOKS_COLUMN_ITEM(NAME) #NAME,
			BOOKS_COLUMN_ITEMS_X_MACRO
#undef		BOOKS_COLUMN_ITEM
};

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

auto ToAuthorItemComparable(const DataItem::Ptr & author)
{
	return std::make_tuple(
		  QStringWrapper { author->GetData(AuthorItem::Column::LastName) }
		, QStringWrapper { author->GetData(AuthorItem::Column::FirstName) }
		, QStringWrapper { author->GetData(AuthorItem::Column::MiddleName) }
	);
}

struct AuthorComparator
{
	bool operator()(const DataItem::Ptr & lhs, const DataItem::Ptr & rhs) const
	{
		return ToAuthorItemComparable(lhs) < ToAuthorItemComparable(rhs);
	}
};

template <typename T>
struct UnorderedSetHash
{
	size_t operator()(const std::unordered_set<T> & key) const
	{
		return std::accumulate(std::cbegin(key), std::cend(key), size_t { 0 }, [] (const size_t init, const T & item)
		{
			return std::rotr(init, 1) ^ std::hash<T>()(item);
		});
	}
};

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

constexpr int BOOKS_QUERY_INDEX_SERIES[] { Book::SeriesId, Book::SeriesTitle };
constexpr int BOOKS_QUERY_INDEX_AUTHOR[] { Book::AuthorId };
constexpr int BOOKS_QUERY_INDEX_GENRE[] { Book::GenreCode, Book::GenreTitle };

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

DataItem::Ptr CreateBooksRoot()
{
	DataItem::Ptr root(BookItem::Create());
	std::ranges::for_each(BOOKS_COLUMN_NAMES, [&, n = 0] (const auto * columnName) mutable
	{
		root->SetData(columnName, n++);
	});
	return root;
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

template<typename KeyType, typename BindType = KeyType>
std::optional<KeyType> UpdateDictionary(std::unordered_map<KeyType, DataItem::Ptr> & dictionary
	, const DB::IQuery & query
	, const QueryInfo & queryInfo
	, const std::function<bool(const DataItem &)> & filter = [] (const DataItem &)
{
	return true;
}
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

template <typename T>
QString Join(const std::unordered_map<T, DataItem::Ptr> & dictionary
	, const std::unordered_set<T> & keyIds
	, const std::function<QString(const DataItem::Ptr &)> & toString
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

QString GetTitle(const DataItem::Ptr & item)
{
	return item->GetData();
}

}

class BooksTreeGenerator::Impl
{
public:
	mutable DataItem::Ptr rootCached;
	const NavigationMode navigationMode;
	const QString navigationId;
	ViewMode viewMode { ViewMode::Unknown };

	Impl(DB::IDatabase & db
		, const NavigationMode navigationMode_
		, QString navigationId_
		, const QueryDescription & description
	)
		: navigationMode(navigationMode_)
		, navigationId(std::move(navigationId_))
	{
		const auto query = db.CreateQuery(QString(BOOKS_QUERY).arg(description.joinClause, description.whereClause).toStdString());
		[[maybe_unused]] const auto result = description.binder(*query, navigationId);
		assert(result == 0);
		for (query->Execute(); !query->Eof(); query->Next())
		{
			auto & book = m_books[query->Get<long long>(Book::BookId)];
			std::get<1>(book) = UpdateDictionary<long long>(m_series, *query, QueryInfo(&CreateSimpleListItem, BOOKS_QUERY_INDEX_SERIES), [] (const DataItem & item)
			{
				return item.GetId() != "-1";
			});
			Add(std::get<2>(book), UpdateDictionary<long long>(m_authors, *query, QueryInfo(&CreateFullAuthorItem, BOOKS_QUERY_INDEX_AUTHOR)));
			Add(std::get<3>(book), UpdateDictionary<QString, const char *>(m_genres, *query, QueryInfo(&CreateSimpleListItem, BOOKS_QUERY_INDEX_GENRE), [] (const DataItem & item)
			{
				return !(item.GetData().isEmpty() || item.GetData()[0].isDigit());
			}));

			if (!std::get<0>(book))
				std::get<0>(book) = CreateBookItem(*query);
		}

		const auto genresComparator = [] (const DataItem::Ptr & lhs, const DataItem::Ptr & rhs)
		{
			return QStringWrapper { lhs->GetId() } < QStringWrapper { rhs->GetId() };
		};

		for (auto & [book, seriesId, authorIds, genreIds] : m_books | std::views::values)
		{
			book->SetData(Join(m_authors, authorIds, &GetAuthorShortName, AuthorComparator {}), BookItem::Column::Author);
			book->SetData(Join(m_genres, genreIds, &GetTitle, genresComparator), BookItem::Column::Genre);
		}
	}

	[[nodiscard]] DataItem::Ptr GetList() const
	{
		DataItem::Items items;
		items.reserve(std::size(m_books));
		std::ranges::transform(m_books | std::views::values, std::back_inserter(items), [] (const auto & item)
		{
			return std::get<0>(item);
		});

		rootCached = CreateBooksRoot();
		rootCached->SetChildren(std::move(items));
		return rootCached;
	}

	[[nodiscard]] DataItem::Ptr CreateAuthorsTree() const
	{
		rootCached = CreateBooksRoot();

		for (const auto & series : m_series | std::views::values)
			rootCached->AppendChild(series);

		for (const auto & [book, seriesId, authorIds, genreIds] : m_books | std::views::values)
		{
			if (!seriesId)
			{
				rootCached->AppendChild(book);
				continue;
			}

			const auto it = m_series.find(*seriesId);
			assert(it != m_series.end());
			it->second->AppendChild(book);
		}

		return rootCached;
	}

	[[nodiscard]] DataItem::Ptr CreateSeriesTree() const
	{
		std::unordered_map<IdsSet, IdsSet, UnorderedSetHash<long long>> authorToBooks;
		for (const auto & [id, book] : m_books)
			authorToBooks[std::get<2>(book)].insert(id);

		rootCached = CreateBooksRoot();
		for (const auto & [authorIds, bookIds] : authorToBooks)
		{
			auto authorsNode = CreateAuthorsNode(authorIds);
			authorsNode->SetChildren(CreateBookItems(bookIds));
			rootCached->AppendChild(std::move(authorsNode));
		}

		return rootCached;
	}

	[[nodiscard]] DataItem::Ptr CreateGeneralTree() const
	{
		using SeriesToBooks = std::unordered_map<long long, IdsSet>;
		std::unordered_map<IdsSet, std::pair<SeriesToBooks, IdsSet>, UnorderedSetHash<long long>> authorToBooks;
		for (const auto & [id, book] : m_books)
		{
			auto & [series, books] = authorToBooks[std::get<2>(book)];
			const auto & seriesId = std::get<1>(book);
			auto & bookIds = seriesId ? series[*seriesId] : books;
			bookIds.insert(id);
		}

		rootCached = CreateBooksRoot();
		for (const auto & [authorIds, authorData] : authorToBooks)
		{
			auto authorsNode = CreateAuthorsNode(authorIds);
			auto & [seriesToBooks, noSeriesBookIds] = authorData;

			for (const auto & [seriesId, bookIds] : seriesToBooks)
			{
				const auto it = m_series.find(seriesId);
				assert(it != m_series.end());
				auto seriesNode = NavigationItem::Create();
				seriesNode->SetData(it->second->GetData());
				seriesNode->SetChildren(CreateBookItems(bookIds));
				authorsNode->AppendChild(std::move(seriesNode));
			}

			for (auto && book : CreateBookItems(noSeriesBookIds))
				authorsNode->AppendChild(std::move(book));

			rootCached->AppendChild(std::move(authorsNode));
		}

		return rootCached;
	}

private:
	DataItem::Items CreateBookItems(const IdsSet & idsSet) const
	{
		DataItem::Items books;
		std::ranges::transform(idsSet, std::back_inserter(books), [&] (const long long id)
		{
			const auto it = m_books.find(id);
			assert(it != m_books.end());
			return std::get<0>(it->second);
		});
		return books;
	}

	DataItem::Ptr CreateAuthorsNode(const IdsSet & idsSet) const
	{
		DataItem::Items authors;
		std::ranges::transform(idsSet, std::back_inserter(authors), [&] (const long long id)
		{
			const auto it = m_authors.find(id);
			assert(it != m_authors.end());
			return it->second;
		});

		std::ranges::sort(authors, AuthorComparator {});
		QString authorsStr;
		for (const auto & author : authors)
		{
			QString authorStr = author->GetData(AuthorItem::Column::LastName);
			AppendTitle(authorStr, author->GetData(AuthorItem::Column::FirstName));
			AppendTitle(authorStr, author->GetData(AuthorItem::Column::MiddleName));
			AppendTitle(authorsStr, authorStr, ", ");
		}

		auto authorsNode = NavigationItem::Create();
		authorsNode->SetData(std::move(authorsStr));
		return authorsNode;
	}

private:
	std::unordered_map<long long, std::tuple<DataItem::Ptr, std::optional<long long>, IdsSet, std::unordered_set<QString>>> m_books;
	std::unordered_map<long long, DataItem::Ptr> m_series;
	std::unordered_map<long long, DataItem::Ptr> m_authors;
	std::unordered_map<QString, DataItem::Ptr> m_genres;
};

BooksTreeGenerator::BooksTreeGenerator(DB::IDatabase & db
	, const NavigationMode navigationMode
	, QString navigationId
	, const QueryDescription & description
)
	: m_impl(db, navigationMode, std::move(navigationId), description)
{
}

BooksTreeGenerator::~BooksTreeGenerator() = default;

NavigationMode BooksTreeGenerator::GetNavigationMode() const noexcept
{
	return m_impl->navigationMode;
}

const QString & BooksTreeGenerator::GetNavigationId() const noexcept
{
	return m_impl->navigationId;
}

ViewMode BooksTreeGenerator::GetBooksViewMode() const noexcept
{
	return m_impl->viewMode;
}

DataItem::Ptr BooksTreeGenerator::GetCached() const noexcept
{
	return m_impl->rootCached;
}

void BooksTreeGenerator::SetBooksViewMode(const ViewMode viewMode) noexcept
{
	m_impl->viewMode = viewMode;
}

// IBooksRootGenerator
[[nodiscard]] DataItem::Ptr BooksTreeGenerator::GetList(Creator) const
{
	return m_impl->GetList();
}

[[nodiscard]] DataItem::Ptr BooksTreeGenerator::GetTree(const Creator creator) const
{
	return ((*this).*creator)();
}

// IBooksTreeCreator
[[nodiscard]] DataItem::Ptr BooksTreeGenerator::CreateAuthorsTree() const
{
	return m_impl->CreateAuthorsTree();
}

[[nodiscard]] DataItem::Ptr BooksTreeGenerator::CreateSeriesTree() const
{
	return m_impl->CreateSeriesTree();
}

[[nodiscard]] DataItem::Ptr BooksTreeGenerator::CreateGeneralTree() const
{
	return m_impl->CreateGeneralTree();
}

namespace HomeCompa::Flibrary {

DataItem::Ptr CreateSimpleListItem(const DB::IQuery & query, const int * index)
{
	auto item = DataItem::Ptr(NavigationItem::Create());

	item->SetId(query.Get<const char *>(index[0]));
	item->SetData(query.Get<const char *>(index[1]));

	return item;
}

void AppendTitle(QString & title, const QString & str, const QString & delimiter)
{
	if (title.isEmpty())
	{
		title = str;
		return;
	}

	if (!str.isEmpty())
		title.append(delimiter).append(str);
}

}
