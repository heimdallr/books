#include "BooksTreeGenerator.h"

#include <numeric>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "fnd/IsOneOf.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITemporaryTable.h"
#include "database/interface/ITransaction.h"

#include "interface/Localization.h"
#include "interface/constants/Enums.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IFilterProvider.h"

#include "database/DatabaseUtil.h"
#include "util/SortString.h"

#include "Constant.h"
#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

using IdsSet = std::unordered_set<long long>;

constexpr const char* BOOKS_COLUMN_NAMES[] {
#define BOOKS_COLUMN_ITEM(NAME) #NAME,
	BOOKS_COLUMN_ITEMS_X_MACRO
#undef BOOKS_COLUMN_ITEM
};

auto ToAuthorItemComparable(const IDataItem::Ptr& author)
{
	return std::make_tuple(
		Util::QStringWrapper { author->GetData(AuthorItem::Column::LastName) },
		Util::QStringWrapper { author->GetData(AuthorItem::Column::FirstName) },
		Util::QStringWrapper { author->GetData(AuthorItem::Column::MiddleName) }
	);
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
		return std::accumulate(std::cbegin(key), std::cend(key), static_cast<size_t>(0), [](const size_t init, const T& item) {
			return std::rotr(init, 1) ^ std::hash<T>()(item);
		});
	}
};

IDataItem::Ptr CreateBooksRoot(const std::vector<const char*>& additionalColumns = {})
{
	IDataItem::Ptr root(BookItem::Create(nullptr, additionalColumns.size()));
	int            n = 0;
	std::ranges::for_each(BOOKS_COLUMN_NAMES, [&](const auto* columnName) mutable {
		root->SetData(columnName, n++);
	});
	std::ranges::for_each(additionalColumns, [&](const auto* columnName) mutable {
		root->SetData(columnName, n++);
	});
	return root;
}

template <typename KeyType>
using UniqueIdList = std::pair<std::unordered_set<KeyType>, std::multimap<int, KeyType>>;

template <typename KeyType>
void Add(UniqueIdList<KeyType>& uniqueIdList, KeyType&& key, const int order)
{
	if (uniqueIdList.first.emplace(key).second)
		uniqueIdList.second.emplace(order, std::forward<KeyType>(key));
}

} // namespace

class BooksTreeGenerator::Impl final : virtual IBookSelector
{
	using SelectedSeries = std::multimap<int, std::pair<long long, int>>;

	struct SelectedBookItem
	{
		IDataItem::Ptr          book;
		SelectedSeries          series;
		UniqueIdList<long long> authors;
		UniqueIdList<QString>   genres;
	};

public:
	mutable IDataItem::Ptr rootCached;
	const NavigationMode   navigationMode;
	const QString          navigationId;
	const IFilterProvider& filterProvider;
	ViewMode               viewMode { ViewMode::Unknown };

	Impl(const Collection& activeCollection, DB::IDatabase& db, const NavigationMode navigationMode, QString navigationId, const QueryDescription& description, const IFilterProvider& filterProvider)
		: navigationMode { navigationMode }
		, navigationId { std::move(navigationId) }
		, filterProvider { filterProvider }
	{
		if (!this->navigationId.isEmpty())
			std::invoke(description.bookSelector, static_cast<IBookSelector&>(*this), std::cref(activeCollection), std::ref(db), std::cref(description));
	}

	[[nodiscard]] IDataItem::Ptr CreateReviewsList() const
	{
		IDataItem::Items items;
		items.reserve(std::size(m_reviews));
		std::ranges::copy(m_reviews | std::views::values, std::back_inserter(items));

		rootCached = CreateBooksRoot({ Loc::READER, Loc::DATE_TIME, Loc::COMMENT });
		rootCached->SetChildren(std::move(items));
		return rootCached;
	}

	[[nodiscard]] IDataItem::Ptr CreateGeneralList() const
	{
		IDataItem::Items items;
		items.reserve(std::size(m_books));
		std::ranges::transform(m_books | std::views::values, std::back_inserter(items), [](const auto& item) {
			return item.book;
		});

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

		for (const auto& [book, seriesIds, authorIds, genreIds] : m_books | std::views::values)
		{
			if (seriesIds.empty())
			{
				rootCached->AppendChild(book);
				continue;
			}

			const auto bookSeriesId = book->GetRawData(BookItem::Column::SeriesId).toLongLong();

			for (const auto& [seriesId, seqNo] : seriesIds | std::views::values)
			{
				const auto it = m_series.find(seriesId);
				assert(it != m_series.end());
				if (bookSeriesId == seriesId)
				{
					it->second->AppendChild(book);
					continue;
				}

				auto clone = book->Clone();
				clone->SetData(QString::number(seriesId), BookItem::Column::SeriesId);
				clone->SetData(it->second->GetData(), BookItem::Column::Series);
				clone->SetData(QString::number(seqNo), BookItem::Column::SeqNumber);
				it->second->AppendChild(std::move(clone));
			}
		}

		return rootCached;
	}

	[[nodiscard]] IDataItem::Ptr CreateSeriesTree() const
	{
		std::unordered_map<IdsSet, IdsSet, UnorderedSetHash<long long>> authorToBooks;
		for (const auto& [id, book] : m_books)
			authorToBooks[book.authors.first].insert(id);

		rootCached = CreateBooksRoot();
		for (const auto& [authorIds, bookIds] : authorToBooks)
		{
			auto authorsNode = CreateAuthorsNode(authorIds);
			authorsNode->SetChildren(CreateBookItems(bookIds, navigationId.toLongLong()));
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

		rootCached = CreateBooksRoot({ Loc::READER, Loc::DATE_TIME, Loc::COMMENT });
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
			auto& [series, books] = authorToBooks[book.authors.first];
			const auto& seriesIds = book.series;
			if (seriesIds.empty())
				books.insert(id);
			else
				for (const auto& seriesId : seriesIds | std::views::values | std::views::keys)
					series[seriesId].insert(id);
		}

		rootCached = CreateBooksRoot();
		for (const auto& [authorIds, authorData] : authorToBooks)
		{
			auto authorsNode                       = CreateAuthorsNode(authorIds);
			auto& [seriesToBooks, noSeriesBookIds] = authorData;

			for (const auto& [seriesId, bookIds] : seriesToBooks)
			{
				const auto it = m_series.find(seriesId);
				assert(it != m_series.end());
				auto seriesNode = NavigationItem::Create();
				seriesNode->SetData(it->second->GetData());
				seriesNode->SetChildren(CreateBookItems(bookIds, seriesId));
				authorsNode->AppendChild(std::move(seriesNode));
			}

			for (auto&& book : CreateBookItems(noSeriesBookIds, -1))
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

		std::ranges::transform(idAuthors.second | std::views::values, std::back_inserter(bookInfo.authors), [&](const auto idAuthor) {
			return m_authors.at(idAuthor);
		});
		std::ranges::transform(idGenres.second | std::views::values, std::back_inserter(bookInfo.genres), [&](const auto& idGenre) {
			return m_genres.at(idGenre);
		});

		return bookInfo;
	}

private: // IBookSelector
	void SelectBooks(const Collection&, DB::IDatabase& db, const QueryDescription& description) override
	{
		CreateSelectedBookItems(db, description.queryClause);
	}

	void SelectReviews(const Collection& activeCollection, DB::IDatabase& db, const QueryDescription& description) override
	{
		const auto folder = activeCollection.GetFolder() + "/" + QString::fromStdWString(Inpx::REVIEWS_FOLDER) + "/" + navigationId + ".7z";
		if (!QFile::exists(folder))
			return;

		const auto tmpTable = db.CreateTemporaryTable({ "Folder VARCHAR (200)", "FileName VARCHAR(170)", "Ext VARCHAR(10)", "ReviewID INTEGER" });

		{
			const auto tr            = db.CreateTransaction();
			const auto insertCommand = tr->CreateCommand(QString("insert into %1(Folder, FileName, Ext, ReviewID) values(?, ?, ?, ?)").arg(tmpTable->GetName().data()).toStdString());
			Zip        zip(folder);
			for (long long reviewId = 0; const auto& file : zip.GetFileNameList())
			{
				auto splitted = file.split('#');
				if (splitted.size() != 2)
					continue;

				QFileInfo  fileInfo(splitted.back());
				const auto bookFolder   = splitted.front().toStdString();
				const auto bookFileName = fileInfo.completeBaseName().toStdString();
				const auto bookExt      = '.' + fileInfo.suffix().toStdString();

				QJsonParseError parseError;
				const auto      doc = QJsonDocument::fromJson(zip.Read(file)->GetStream().readAll(), &parseError);
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
					insertCommand->Bind(0, bookFolder);
					insertCommand->Bind(1, bookFileName);
					insertCommand->Bind(2, bookExt);
					insertCommand->Bind(3, ++reviewId);
					insertCommand->Execute();

					auto& reviewItem = m_reviews.try_emplace(reviewId, ReviewItem::Create()).first->second;
					reviewItem->SetData(recordObject[Inpx::NAME].toString(), ReviewItem::Column::Name);
					reviewItem->SetData(recordObject[Inpx::TIME].toString(), ReviewItem::Column::Time);
					reviewItem->SetData(recordObject[Inpx::TEXT].toString(), ReviewItem::Column::Comment);
					if (reviewItem->GetData(ReviewItem::Column::Name).isEmpty())
						reviewItem->SetData(Loc::Tr(Loc::Ctx::COMMON, Loc::ANONYMOUS), ReviewItem::Column::Name);
				}
			}
			tr->Commit();
		}

		auto       queryClause     = description.queryClause;
		const auto booksFrom       = QString(queryClause.booksFrom).arg(tmpTable->GetName()).toStdString();
		const auto navigationFrom  = QString(queryClause.navigationFrom).arg(tmpTable->GetName()).toStdString();
		queryClause.booksFrom      = booksFrom.data();
		queryClause.navigationFrom = navigationFrom.data();

		CreateSelectedBookItems(db, queryClause, [&](const DB::IQuery& query, const SelectedBookItem& selectedItem) {
			const auto it = m_reviews.find(query.Get<long long>(BookQueryFields::Last));
			assert(it != m_reviews.end());

			auto&       reviewItem = it->second;
			const auto& bookItem   = reviewItem->AppendChild(selectedItem.book);
			reviewItem->SetId(bookItem->GetId());
		});

		const auto orphans = m_reviews | std::views::filter([](const auto& item) {
								 return item.second->GetChildCount() == 0;
							 })
		                   | std::views::keys | std::ranges::to<std::vector<long long>>();
		for (const auto id : orphans)
			m_reviews.erase(id);
	}

private:
	bool IsFiltered(const IDataItem& item) const
	{
		return filterProvider.IsFilterEnabled() && !!(item.GetFlags() & IDataItem::Flags::Filtered);
	}

	template <typename T, std::ranges::range U>
	QString Join(const std::unordered_map<T, IDataItem::Ptr>& dictionary, const U& keyIds)
	{
		IDataItem::Items values;
		values.reserve(std::size(keyIds));
		std::ranges::transform(keyIds, std::back_inserter(values), [&](const T& id) {
			const auto it = dictionary.find(id);
			assert(it != dictionary.end());
			return it->second;
		});

		QString result;
		for (const auto& value : values | std::views::filter([this](const auto& item) {
									 return !IsFiltered(*item);
								 }))
			AppendTitle(result, value->GetData(0), ", ");

		return result;
	}

	void CreateSelectedBookItems(
		DB::IDatabase&                                                         db,
		const QueryClause&                                                     queryClause,
		const std::function<void(const DB::IQuery&, const SelectedBookItem&)>& additional =
			[](const DB::IQuery&, const auto&) {
			}
	)
	{
		const auto with = queryClause.with && queryClause.with[0] ? QString(queryClause.with).arg(navigationId).toStdString() : std::string {};

		{
			QString booksWhere = queryClause.booksWhere;
			if (!booksWhere.isEmpty())
				booksWhere = booksWhere.arg(navigationId);

			const auto query = db.CreateQuery(std::format(DatabaseUtil::BOOKS_QUERY, with, queryClause.additionalFields, queryClause.booksFrom, booksWhere.toStdString()));
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto& book = m_books[query->Get<long long>(BookQueryFields::BookId)];
				if (!book.book)
					book.book = DatabaseUtil::CreateBookItem(*query);
				additional(*query, book);
			}
		}

		const auto addFlag = [](IDataItem& book, const IDataItem::Flags flags) {
			if (!(flags & IDataItem::Flags::BooksFiltered))
				return;

			auto toSet = IDataItem::Flags::Filtered;
			if (!!(book.GetFlags() & IDataItem::Flags::Filtered))
				toSet |= IDataItem::Flags::Multiple;
			book.SetFlags(toSet);
		};

		auto where = [&] {
			QString result(queryClause.navigationWhere);
			if (!result.isEmpty())
				result = result.arg(navigationId);
			return result.toStdString();
		}();

		{
			static constexpr auto queryText = R"({}
select s.SeriesID, s.SeriesTitle, s.IsDeleted, s.Flags, l.BookID, l.SeqNumber, l.OrdNum
{}
join Series_List l on l.BookID = b.BookID
join Series s on s.SeriesID = l.SeriesID
{}
)";
			const auto            query     = db.CreateQuery(std::format(queryText, with, queryClause.navigationFrom, where));
			for (query->Execute(); !query->Eof(); query->Next())
			{
				const auto id   = query->Get<long long>(0);
				auto&      item = m_series[id];
				if (!item)
					item = DatabaseUtil::CreateSimpleListItem(*query);

				auto& book = m_books[query->Get<long long>(4)];
				assert(book.book);
				addFlag(*book.book, item->GetFlags());

				if (IsFiltered(*item))
					continue;

				const auto seqNum = query->Get<int>(5);
				const auto ordNum = query->Get<int>(6);
				book.series.emplace(ordNum, std::make_pair(id, seqNum));
			}
		}

		{
			static constexpr auto queryText = R"({}
select a.AuthorID, a.LastName, a.FirstName, a.MiddleName, a.IsDeleted, a.Flags, l.BookID, l.OrdNum
{}
join Author_List l on l.BookID = b.BookID
join Authors a on a.AuthorID = l.AuthorID
{}
)";
			const auto            query     = db.CreateQuery(std::format(queryText, with, queryClause.navigationFrom, where));
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto  id   = query->Get<long long>(0);
				auto& item = m_authors[id];
				if (!item)
					item = DatabaseUtil::CreateFullAuthorItem(*query);

				auto& book = m_books[query->Get<long long>(6)];
				assert(book.book);
				Add(book.authors, static_cast<long long&&>(id), query->Get<int>(7));
				addFlag(*book.book, item->GetFlags());
			}
		}
		{
			static constexpr auto queryText = R"({}
select g.GenreCode, g.GenreAlias, g.FB2Code, g.IsDeleted, g.Flags, l.BookID, l.OrdNum
{}
join Genre_List l on l.BookID = b.BookID
join Genres g on g.GenreCode = l.GenreCode
{}
)";
			const auto            query     = db.CreateQuery(std::format(queryText, with, queryClause.navigationFrom, where));
			for (query->Execute(); !query->Eof(); query->Next())
			{
				QString id   = query->Get<const char*>(0);
				auto&   item = m_genres[id];
				if (!item)
					item = DatabaseUtil::CreateGenreItem(*query);

				auto& book = m_books[query->Get<long long>(5)];
				assert(book.book);
				Add(book.genres, std::move(id), query->Get<int>(6));
				addFlag(*book.book, item->GetFlags());
			}
		}
		{
			static constexpr auto queryText = R"({}
select k.Flags, l.BookID, l.OrdNum
{}
join Keyword_List l on l.BookID = b.BookID
join Keywords k on k.KeywordID = l.KeywordID
{}
)";
			const auto            query     = db.CreateQuery(std::format(queryText, with, queryClause.navigationFrom, where));
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto& book = m_books[query->Get<long long>(1)];
				assert(book.book);
				addFlag(*book.book, static_cast<IDataItem::Flags>(query->Get<int>(0)));
			}
		}

		for (auto& [book, seriesId, authorIds, genreIds] : m_books | std::views::values)
		{
			for (const auto authorId : authorIds.second | std::views::values)
			{
				const auto it = m_authors.find(authorId);
				assert(it != m_authors.end());
				const auto& author = *it->second;
				if (!IsFiltered(author))
				{
					auto authorStr = GetAuthorFull(author);
					book->SetData(std::move(authorStr), BookItem::Column::AuthorFull);
					break;
				}
			}
		}

		for (const auto& author : m_authors | std::views::values)
			author->Reduce();

		for (auto& [book, seriesIds, authorIds, genreIds] : m_books | std::views::values)
		{
			book->SetData(std::size(authorIds.second) > 1 ? Join(m_authors, authorIds.second | std::views::values) : book->GetRawData(BookItem::Column::AuthorFull), BookItem::Column::Author);
			book->SetData(Join(m_genres, genreIds.second | std::views::values), BookItem::Column::Genre);
			if (seriesIds.empty())
				continue;

			const auto& [seriesId, seqNum] = seriesIds.cbegin()->second;
			book->SetData(QString::number(seriesId), BookItem::Column::SeriesId);
			book->SetData(seqNum > 0 ? QString::number(seqNum) : QString {}, BookItem::Column::SeqNumber);
			book->SetData(m_series.find(seriesId)->second->GetData(), BookItem::Column::Series);
		}
	}

	IDataItem::Items CreateBookItems(const IdsSet& idsSet, const long long seriesId) const
	{
		const auto seriesIt = m_series.find(seriesId);

		IDataItem::Items books;
		std::ranges::transform(idsSet, std::back_inserter(books), [&](const long long id) {
			const auto it = m_books.find(id);
			assert(it != m_books.end());

			if (IsOneOf(it->second.book->GetData(BookItem::Column::SeriesId).toLongLong(), seriesId, -1))
				return it->second.book;

			const auto bookSeriesIt = std::ranges::find(it->second.series, seriesId, [](const auto& item) {
				return item.second.first;
			});
			assert(bookSeriesIt != it->second.series.end() && seriesIt != m_series.end());

			auto clone = it->second.book->Clone();
			clone->SetData(QString::number(seriesId), BookItem::Column::SeriesId);
			clone->SetData(seriesIt->second->GetData(), BookItem::Column::Series);
			clone->SetData(QString::number(bookSeriesIt->second.second), BookItem::Column::SeqNumber);

			return clone;
		});
		return books;
	}

	IDataItem::Ptr CreateAuthorsNode(const IdsSet& idsSet) const
	{
		IDataItem::Items authors;
		std::ranges::transform(idsSet, std::back_inserter(authors), [&](const long long id) {
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
	std::unordered_map<long long, IDataItem::Ptr>   m_reviews;
	std::unordered_map<long long, SelectedBookItem> m_books;
	std::unordered_map<long long, IDataItem::Ptr>   m_series;
	std::unordered_map<long long, IDataItem::Ptr>   m_authors;
	std::unordered_map<QString, IDataItem::Ptr>     m_genres;
};

BooksTreeGenerator::BooksTreeGenerator(
	const Collection&       activeCollection,
	DB::IDatabase&          db,
	const NavigationMode    navigationMode,
	QString                 navigationId,
	const QueryDescription& description,
	const IFilterProvider&  filterProvider
)
	: m_impl(activeCollection, db, navigationMode, std::move(navigationId), description, filterProvider)
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
