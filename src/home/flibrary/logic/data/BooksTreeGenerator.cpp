#include "BooksTreeGenerator.h"

#include <numeric>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITemporaryTable.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"

#include "database/DatabaseUtil.h"
#include "inpx/src/util/constant.h"
#include "util/SortString.h"
#include "util/localization.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "BooksTreeGenerator";
constexpr auto ANONYMOUS = QT_TRANSLATE_NOOP("BooksTreeGenerator", "Anonymous");
TR_DEF

using IdsSet = std::unordered_set<long long>;

constexpr const char* BOOKS_COLUMN_NAMES[] {
#define BOOKS_COLUMN_ITEM(NAME) #NAME,
	BOOKS_COLUMN_ITEMS_X_MACRO
#undef BOOKS_COLUMN_ITEM
};

constexpr auto BOOKS_QUERY = R"(
%4select
	%1,
	a.AuthorID, a.LastName, a.FirstName, a.MiddleName, g.GenreCode, g.GenreAlias, g.FB2Code, coalesce(b.SeriesID, -1), s.SeriesTitle %5
		from Books b
		join Author_List al on al.BookID = b.BookID
		join Authors a on a.AuthorID = al.AuthorID
		join Genre_List gl on gl.BookID = b.BookID
		join Genres g on g.GenreCode = gl.GenreCode
		join Folders f on f.FolderID = b.FolderID
		%2
		left join Series s on s.SeriesID = b.SeriesID
		left join Books_User bu on bu.BookID = b.BookID
		%3
		order by b.BookID, al.OrdNum, gl.OrdNum
)";

auto ToAuthorItemComparable(const IDataItem::Ptr& author)
{
	return std::make_tuple(Util::QStringWrapper { author->GetData(AuthorItem::Column::LastName) },
	                       Util::QStringWrapper { author->GetData(AuthorItem::Column::FirstName) },
	                       Util::QStringWrapper { author->GetData(AuthorItem::Column::MiddleName) });
}

struct AuthorComparator
{
	bool operator()(const IDataItem::Ptr& lhs, const IDataItem::Ptr& rhs) const
	{
		return ToAuthorItemComparable(lhs) < ToAuthorItemComparable(rhs);
	}
};

template <typename T>
struct UnorderedSetHash
{
	size_t operator()(const std::unordered_set<T>& key) const
	{
		return std::accumulate(std::cbegin(key), std::cend(key), static_cast<size_t>(0), [](const size_t init, const T& item) { return std::rotr(init, 1) ^ std::hash<T>()(item); });
	}
};

constexpr size_t BOOK_QUERY_TO_AUTHOR[] {
	BookQueryFields::AuthorID, BookQueryFields::AuthorLastName, BookQueryFields::AuthorLastName, BookQueryFields::AuthorFirstName, BookQueryFields::AuthorMiddleName,
};

constexpr size_t BOOKS_QUERY_INDEX_SERIES[] { BookQueryFields::SeriesId, BookQueryFields::SeriesTitle };
constexpr size_t BOOKS_QUERY_INDEX_GENRE[] { BookQueryFields::GenreCode, BookQueryFields::GenreTitle, BookQueryFields::GenreFB2Code };

IDataItem::Ptr CreateBooksRoot(const std::vector<const char*>& additionalColumns = {})
{
	IDataItem::Ptr root(BookItem::Create(nullptr, additionalColumns.size()));
	int n = 0;
	std::ranges::for_each(BOOKS_COLUMN_NAMES, [&](const auto* columnName) mutable { root->SetData(columnName, n++); });
	std::ranges::for_each(additionalColumns, [&](const auto* columnName) mutable { root->SetData(columnName, n++); });
	return root;
}

template <typename KeyType, typename BindType = KeyType>
std::optional<KeyType> UpdateDictionary(
	std::unordered_map<KeyType, IDataItem::Ptr>& dictionary,
	const DB::IQuery& query,
	const QueryInfo& queryInfo,
	const std::function<bool(const IDataItem&)>& filter = [](const IDataItem&) { return true; })
{
	auto key = query.Get<BindType>(queryInfo.index[0]);
	const auto it = dictionary.find(key);
	if (it != dictionary.end())
		return it->first;

	auto item = queryInfo.extractor(query, queryInfo.index, queryInfo.removedIndex);
	return filter(*item) ? std::optional<KeyType>(dictionary.emplace(key, std::move(item)).first->first) : std::nullopt;
}

template <typename KeyType>
using UniqueIdList = std::pair<std::unordered_set<KeyType>, std::vector<KeyType>>;

template <typename KeyType>
void Add(UniqueIdList<KeyType>& uniqueIdList, std::optional<KeyType> key)
{
	if (key && uniqueIdList.first.emplace(*key).second)
		uniqueIdList.second.emplace_back(*key);
}

template <typename T>
QString Join(const std::unordered_map<T, IDataItem::Ptr>& dictionary, const std::vector<T>& keyIds)
{
	IDataItem::Items values;
	values.reserve(std::size(keyIds));
	std::ranges::transform(keyIds,
	                       std::back_inserter(values),
	                       [&](const T& id)
	                       {
							   const auto it = dictionary.find(id);
							   assert(it != dictionary.end());
							   return it->second;
						   });

	QString result;
	for (const auto& value : values)
		AppendTitle(result, value->GetData(0), ", ");

	return result;
}

} // namespace

class BooksTreeGenerator::Impl final : virtual IBookSelector
{
	using SelectedBookItem = std::tuple<IDataItem::Ptr, std::optional<long long>, UniqueIdList<long long>, UniqueIdList<QString>>;

public:
	mutable IDataItem::Ptr rootCached;
	const NavigationMode navigationMode;
	const QString navigationId;
	ViewMode viewMode { ViewMode::Unknown };

	Impl(const Collection& activeCollection, DB::IDatabase& db, const NavigationMode navigationMode, QString navigationId, const QueryDescription& description)
		: navigationMode { navigationMode }
		, navigationId { std::move(navigationId) }
	{
		if (!this->navigationId.isEmpty())
			std::invoke(description.bookSelector, static_cast<IBookSelector&>(*this), std::cref(activeCollection), std::ref(db), std::cref(description));
	}

	[[nodiscard]] IDataItem::Ptr CreateReviewsList() const
	{
		IDataItem::Items items;
		items.reserve(std::size(m_reviews));
		std::ranges::copy(m_reviews | std::views::values, std::back_inserter(items));

		rootCached = CreateBooksRoot({ Loc::READER, Loc::TIME, Loc::COMMENT });
		rootCached->SetChildren(std::move(items));
		return rootCached;
	}

	[[nodiscard]] IDataItem::Ptr CreateGeneralList() const
	{
		IDataItem::Items items;
		items.reserve(std::size(m_books));
		std::ranges::transform(m_books | std::views::values, std::back_inserter(items), [](const auto& item) { return std::get<0>(item); });

		rootCached = CreateBooksRoot();
		rootCached->SetChildren(std::move(items));
		return rootCached;
	}

	[[nodiscard]] IDataItem::Ptr CreateAuthorsTree() const
	{
		rootCached = CreateBooksRoot();

		for (const auto& series : m_series | std::views::values)
		{
			series->RemoveAllChildren();
			rootCached->AppendChild(series);
		}

		for (const auto& [book, seriesId, authorIds, genreIds] : m_books | std::views::values)
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

	[[nodiscard]] IDataItem::Ptr CreateSeriesTree() const
	{
		std::unordered_map<IdsSet, IdsSet, UnorderedSetHash<long long>> authorToBooks;
		for (const auto& [id, book] : m_books)
			authorToBooks[std::get<2>(book).first].insert(id);

		rootCached = CreateBooksRoot();
		for (const auto& [authorIds, bookIds] : authorToBooks)
		{
			auto authorsNode = CreateAuthorsNode(authorIds);
			authorsNode->SetChildren(CreateBookItems(bookIds));
			rootCached->AppendChild(std::move(authorsNode));
		}

		return rootCached;
	}

	[[nodiscard]] IDataItem::Ptr CreateReviewsTree() const
	{
		std::unordered_map<QString, IDataItem::Ptr> reviewers;
		for (const auto& reviewItem : m_reviews | std::views::values)
		{
			auto& reviewer = reviewers[reviewItem->GetData(ReviewItem::Column::Name)];
			if (!reviewer)
			{
				reviewer = NavigationItem::Create();
				reviewer->SetData(reviewItem->GetData(ReviewItem::Column::Name));
			}

			reviewer->AppendChild(reviewItem);
		}

		rootCached = CreateBooksRoot({ Loc::READER, Loc::TIME, Loc::COMMENT });
		for (auto&& reviewer : reviewers | std::views::values)
			rootCached->AppendChild(std::move(reviewer));
		return rootCached;
	}

	[[nodiscard]] IDataItem::Ptr CreateGeneralTree() const
	{
		using SeriesToBooks = std::unordered_map<long long, IdsSet>;
		std::unordered_map<IdsSet, std::pair<SeriesToBooks, IdsSet>, UnorderedSetHash<long long>> authorToBooks;
		for (const auto& [id, book] : m_books)
		{
			auto& [series, books] = authorToBooks[std::get<2>(book).first];
			const auto& seriesId = std::get<1>(book);
			auto& bookIds = seriesId ? series[*seriesId] : books;
			bookIds.insert(id);
		}

		rootCached = CreateBooksRoot();
		for (const auto& [authorIds, authorData] : authorToBooks)
		{
			auto authorsNode = CreateAuthorsNode(authorIds);
			auto& [seriesToBooks, noSeriesBookIds] = authorData;

			for (const auto& [seriesId, bookIds] : seriesToBooks)
			{
				const auto it = m_series.find(seriesId);
				assert(it != m_series.end());
				auto seriesNode = NavigationItem::Create();
				seriesNode->SetData(it->second->GetData());
				seriesNode->SetChildren(CreateBookItems(bookIds));
				authorsNode->AppendChild(std::move(seriesNode));
			}

			for (auto&& book : CreateBookItems(noSeriesBookIds))
				authorsNode->AppendChild(std::move(book));

			rootCached->AppendChild(std::move(authorsNode));
		}

		return rootCached;
	}

	[[nodiscard]] BookInfo GetBookInfo(const long long id) const
	{
		const auto it = m_books.find(id);
		assert(it != m_books.end());
		const auto& [book, idSeries, idAuthors, idGenres] = it->second;

		BookInfo bookInfo { book };

		std::ranges::transform(idAuthors.second, std::back_inserter(bookInfo.authors), [&](const auto idAuthor) { return m_authors.at(idAuthor); });
		std::ranges::transform(idGenres.second, std::back_inserter(bookInfo.genres), [&](const auto& idGenre) { return m_genres.at(idGenre); });

		return bookInfo;
	}

private: // IBookSelector
	void SelectBooks(const Collection&, DB::IDatabase& db, const QueryDescription& description) override
	{
		const auto queryText =
			QString(BOOKS_QUERY).arg(QString(IDatabaseUser::BOOKS_QUERY_FIELDS).arg(description.seqNumberTableAlias)).arg(description.joinClause, description.whereClause, description.with, "").toStdString();
		const auto query = db.CreateQuery(queryText);
		[[maybe_unused]] const auto result = description.binder(*query, navigationId);
		assert(result == 0);
		CreateSelectedBookItems(*query);
	}

	void SelectReviews(const Collection& activeCollection, DB::IDatabase& db, const QueryDescription& description) override
	{
		const auto folder = activeCollection.folder + "/" + QString::fromStdWString(REVIEWS_FOLDER) + "/" + navigationId + ".7z";
		if (!QFile::exists(folder))
			return;

		const auto tr = db.CreateTransaction();
		const auto tmpTable = tr->CreateTemporaryTable({ "LibID VARCHAR (200)", "Name VARCHAR (200)", "Time VARCHAR (20)", "Text BLOB (1024)", "ReviewID INTEGER" });

		{
			const auto insertCommand = tr->CreateCommand(QString("insert into %1(LibID, Name, Time, Text, ReviewID) values(?, ?, ?, ?, ?)").arg(tmpTable->GetName().data()).toStdString());
			Zip zip(folder);
			for (int reviewId = 0; const auto& file : zip.GetFileNameList())
			{
				QJsonParseError parseError;
				const auto doc = QJsonDocument::fromJson(zip.Read(file)->GetStream().readAll(), &parseError);
				if (parseError.error != QJsonParseError::NoError)
				{
					PLOGW << parseError.errorString();
					continue;
				}

				assert(doc.isArray());
				for (const auto recordValue : doc.array())
				{
					assert(recordValue.isObject());
					const auto recordObject = recordValue.toObject();
					insertCommand->Bind(0, file.toStdString());
					insertCommand->Bind(1, recordObject["name"].toString().toStdString());
					insertCommand->Bind(2, recordObject["time"].toString().toStdString());
					insertCommand->Bind(3, recordObject["text"].toString().toStdString());
					insertCommand->Bind(4, ++reviewId);
					insertCommand->Execute();
				}
			}
		}

		const auto queryText = QString(BOOKS_QUERY)
		                           .arg(QString(IDatabaseUser::BOOKS_QUERY_FIELDS).arg(description.seqNumberTableAlias))
		                           .arg(QString("join %1 t on t.LibID = b.LibID").arg(tmpTable->GetName().data()), "", "", ", t.Name, t.Time, t.Text, t.ReviewID")
		                           .toStdString();
		const auto query = tr->CreateQuery(queryText);
		CreateSelectedBookItems(*query,
		                        [&](const SelectedBookItem& selectedItem)
		                        {
									auto& reviewItem = m_reviews[query->Get<long long>(static_cast<int>(BookQueryFields::Last) + static_cast<int>(ReviewItem::Column::Last))];
									if (reviewItem)
										return;

									reviewItem = ReviewItem::Create();
									const auto bookItem = reviewItem->AppendChild(std::get<0>(selectedItem));
									reviewItem->SetId(bookItem->GetId());
									for (int i = 0; i < ReviewItem::Column::Last; ++i)
										reviewItem->SetData(query->Get<const char*>(BookQueryFields::Last + i), i);
									if (reviewItem->GetData(ReviewItem::Column::Name).isEmpty())
										reviewItem->SetData(Tr(ANONYMOUS), ReviewItem::Column::Name);
								});
	}

private:
	void CreateSelectedBookItems(DB::IQuery& query, const std::function<void(SelectedBookItem&)>& additional = [](auto&) {})
	{
		for (query.Execute(); !query.Eof(); query.Next())
		{
			auto& book = m_books[query.Get<long long>(BookQueryFields::BookId)];
			std::get<1>(book) =
				UpdateDictionary<long long>(m_series, query, QueryInfo { &DatabaseUtil::CreateSimpleListItem, BOOKS_QUERY_INDEX_SERIES }, [](const IDataItem& item) { return item.GetId() != "-1"; });
			Add(std::get<2>(book), UpdateDictionary<long long>(m_authors, query, QueryInfo { &DatabaseUtil::CreateFullAuthorItem, BOOK_QUERY_TO_AUTHOR }));
			Add(std::get<3>(book),
			    UpdateDictionary<QString, const char*>(m_genres,
			                                           query,
			                                           QueryInfo { &DatabaseUtil::CreateGenreItem, BOOKS_QUERY_INDEX_GENRE },
			                                           [](const IDataItem& item) { return !(item.GetData().isEmpty() || item.GetData()[0].isDigit()); }));

			if (!std::get<0>(book))
				std::get<0>(book) = DatabaseUtil::CreateBookItem(query);

			additional(book);
		}

		for (auto& [book, seriesId, authorIds, genreIds] : m_books | std::views::values)
		{
			assert(!authorIds.second.empty());
			const auto& author = m_authors.find(authorIds.second.front())->second;
			auto authorStr = GetAuthorFull(*author);
			book->SetData(std::move(authorStr), BookItem::Column::AuthorFull);
		}

		for (const auto& author : m_authors | std::views::values)
			author->Reduce();

		for (auto& [book, seriesId, authorIds, genreIds] : m_books | std::views::values)
		{
			book->SetData(std::size(authorIds.second) > 1 ? Join(m_authors, authorIds.second) : book->GetRawData(BookItem::Column::AuthorFull), BookItem::Column::Author);
			book->SetData(Join(m_genres, genreIds.second), BookItem::Column::Genre);
		}
	}

	IDataItem::Items CreateBookItems(const IdsSet& idsSet) const
	{
		IDataItem::Items books;
		std::ranges::transform(idsSet,
		                       std::back_inserter(books),
		                       [&](const long long id)
		                       {
								   const auto it = m_books.find(id);
								   assert(it != m_books.end());
								   return std::get<0>(it->second);
							   });
		return books;
	}

	IDataItem::Ptr CreateAuthorsNode(const IdsSet& idsSet) const
	{
		IDataItem::Items authors;
		std::ranges::transform(idsSet,
		                       std::back_inserter(authors),
		                       [&](const long long id)
		                       {
								   const auto it = m_authors.find(id);
								   assert(it != m_authors.end());
								   return it->second;
							   });

		std::ranges::sort(authors, AuthorComparator {});
		QString authorsStr;
		for (const auto& author : authors)
		{
			const auto authorStr = GetAuthorFull(*author);
			AppendTitle(authorsStr, authorStr, ", ");
		}

		auto authorsNode = NavigationItem::Create();
		authorsNode->SetData(std::move(authorsStr));
		return authorsNode;
	}

private:
	std::unordered_map<long long, IDataItem::Ptr> m_reviews;
	std::unordered_map<long long, SelectedBookItem> m_books;
	std::unordered_map<long long, IDataItem::Ptr> m_series;
	std::unordered_map<long long, IDataItem::Ptr> m_authors;
	std::unordered_map<QString, IDataItem::Ptr> m_genres;
};

BooksTreeGenerator::BooksTreeGenerator(const Collection& activeCollection, DB::IDatabase& db, const NavigationMode navigationMode, QString navigationId, const QueryDescription& description)
	: m_impl(activeCollection, db, navigationMode, std::move(navigationId), description)
{
	PLOGV << "BooksTreeGenerator created";
}

BooksTreeGenerator::~BooksTreeGenerator()
{
	PLOGV << "BooksTreeGenerator destroyed";
}

NavigationMode BooksTreeGenerator::GetNavigationMode() const noexcept
{
	return m_impl->navigationMode;
}

const QString& BooksTreeGenerator::GetNavigationId() const noexcept
{
	return m_impl->navigationId;
}

ViewMode BooksTreeGenerator::GetBooksViewMode() const noexcept
{
	return m_impl->viewMode;
}

IDataItem::Ptr BooksTreeGenerator::GetCached() const noexcept
{
	return m_impl->rootCached;
}

void BooksTreeGenerator::SetBooksViewMode(const ViewMode viewMode) noexcept
{
	m_impl->viewMode = viewMode;
}

BookInfo BooksTreeGenerator::GetBookInfo(const long long id) const
{
	return m_impl->GetBookInfo(id);
}

// IBooksRootGenerator
IDataItem::Ptr BooksTreeGenerator::GetList(const QueryDescription& queryDescription) const
{
	return std::invoke(queryDescription.listCreator, static_cast<const IBooksListCreator&>(*this));
}

IDataItem::Ptr BooksTreeGenerator::GetTree(const QueryDescription& queryDescription) const
{
	return std::invoke(queryDescription.treeCreator, static_cast<const IBooksTreeCreator&>(*this));
}

// IBooksListCreator
IDataItem::Ptr BooksTreeGenerator::CreateReviewsList() const
{
	return m_impl->CreateReviewsList();
}

IDataItem::Ptr BooksTreeGenerator::CreateGeneralList() const
{
	return m_impl->CreateGeneralList();
}

// IBooksTreeCreator
IDataItem::Ptr BooksTreeGenerator::CreateAuthorsTree() const
{
	return m_impl->CreateAuthorsTree();
}

IDataItem::Ptr BooksTreeGenerator::CreateSeriesTree() const
{
	return m_impl->CreateSeriesTree();
}

IDataItem::Ptr BooksTreeGenerator::CreateReviewsTree() const
{
	return m_impl->CreateReviewsTree();
}

IDataItem::Ptr BooksTreeGenerator::CreateGeneralTree() const
{
	return m_impl->CreateGeneralTree();
}
