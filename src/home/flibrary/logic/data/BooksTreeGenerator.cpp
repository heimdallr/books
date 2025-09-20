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
#include "interface/constants/ProductConstant.h"
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

using IdsSet = std::unordered_set<long long>;

constexpr const char* BOOKS_COLUMN_NAMES[] {
#define BOOKS_COLUMN_ITEM(NAME) #NAME,
	BOOKS_COLUMN_ITEMS_X_MACRO
#undef BOOKS_COLUMN_ITEM
};

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

IDataItem::Ptr CreateBooksRoot(const std::vector<const char*>& additionalColumns = {})
{
	IDataItem::Ptr root(BookItem::Create(nullptr, additionalColumns.size()));
	int n = 0;
	std::ranges::for_each(BOOKS_COLUMN_NAMES, [&](const auto* columnName) mutable { root->SetData(columnName, n++); });
	std::ranges::for_each(additionalColumns, [&](const auto* columnName) mutable { root->SetData(columnName, n++); });
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

template <typename T, std::ranges::range U>
QString Join(const std::unordered_map<T, IDataItem::Ptr>& dictionary, const U& keyIds)
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

		rootCached = CreateBooksRoot({ Loc::READER, Loc::DATE_TIME, Loc::COMMENT });
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

		std::ranges::transform(idAuthors.second | std::views::values, std::back_inserter(bookInfo.authors), [&](const auto idAuthor) { return m_authors.at(idAuthor); });
		std::ranges::transform(idGenres.second | std::views::values, std::back_inserter(bookInfo.genres), [&](const auto& idGenre) { return m_genres.at(idGenre); });

		return bookInfo;
	}

private: // IBookSelector
	void SelectBooks(const Collection&, DB::IDatabase& db, const QueryDescription& description) override
	{
		CreateSelectedBookItems(db, description.queryClause);
	}

	void SelectReviews(const Collection& activeCollection, DB::IDatabase& db, const QueryDescription& description) override
	{
		const auto folder = activeCollection.folder + "/" + QString::fromStdWString(REVIEWS_FOLDER) + "/" + navigationId + ".7z";
		if (!QFile::exists(folder))
			return;

		const auto tmpTable = db.CreateTemporaryTable({ "LibID VARCHAR (200)", "ReviewID INTEGER" });

		{
			const auto tr = db.CreateTransaction();
			const auto insertCommand = tr->CreateCommand(QString("insert into %1(LibID, ReviewID) values(?, ?)").arg(tmpTable->GetName().data()).toStdString());
			Zip zip(folder);
			for (long long reviewId = 0; const auto& file : zip.GetFileNameList())
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
					insertCommand->Bind(1, ++reviewId);
					insertCommand->Execute();

					auto& reviewItem = m_reviews.try_emplace(reviewId, ReviewItem::Create()).first->second;
					reviewItem->SetData(recordObject[Constant::NAME].toString(), ReviewItem::Column::Name);
					reviewItem->SetData(recordObject[Constant::TIME].toString(), ReviewItem::Column::Time);
					reviewItem->SetData(recordObject[Constant::TEXT].toString(), ReviewItem::Column::Comment);
					if (reviewItem->GetData(ReviewItem::Column::Name).isEmpty())
						reviewItem->SetData(Loc::Tr(Loc::Ctx::COMMON, Loc::ANONYMOUS), ReviewItem::Column::Name);
				}
			}
//			tr->CreateCommand(std::format("CREATE INDEX IX_LibID ON {} (LibID)", tmpTable->GetName()))->Execute();
			tr->Commit();
		}

		CreateSelectedBookItems(db,
		                        description.queryClause,
		                        [&](const DB::IQuery& query, const SelectedBookItem& selectedItem)
		                        {
									const auto it = m_reviews.find(query.Get<long long>(BookQueryFields::Last));
									assert(it != m_reviews.end());

									auto& reviewItem = it->second;
									const auto& bookItem = reviewItem->AppendChild(std::get<0>(selectedItem));
									reviewItem->SetId(bookItem->GetId());
								});
	}

private:
	void CreateSelectedBookItems(
		DB::IDatabase& db,
		const QueryClause& queryClause,
		const std::function<void(const DB::IQuery&, const SelectedBookItem&)>& additional = [](const DB::IQuery&, const auto&) {})
	{
		const auto with = QString(queryClause.with).arg(navigationId).toStdString();

		{
			static constexpr auto queryText = R"({}
select b.BookID, b.Title, coalesce(b.SeqNumber, -1) SeqNumber, b.UpdateDate, b.LibRate, b.Lang, b.Year, f.FolderTitle, b.FileName, b.BookSize, b.UserRate, b.LibID, b.IsDeleted, l.Flags{}
    {}
    join Folders f on f.FolderID = b.FolderID
    join Languages l on l.LanguageCode = b.Lang
	{}
)";
			const auto query = db.CreateQuery(std::format(queryText, with, queryClause.additionalFields, queryClause.booksFrom, QString(queryClause.booksWhere).arg(navigationId).toStdString()));
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto& book = m_books[query->Get<long long>(BookQueryFields::BookId)];
				if (!std::get<0>(book))
					std::get<0>(book) = DatabaseUtil::CreateBookItem(*query);
				additional(*query, book);
			}
		}

		const auto addFlag = [](IDataItem& book, const IDataItem::Flags flags)
		{
			if (!(flags & IDataItem::Flags::BooksFiltered))
				return;

			auto toSet = IDataItem::Flags::Filtered;
			if (!!(book.GetFlags() & IDataItem::Flags::Filtered))
				toSet |= IDataItem::Flags::Multiple;
			book.SetFlags(toSet);
		};

		const auto where = QString(queryClause.navigationWhere).arg(navigationId).toStdString();

		{
			static constexpr auto queryText = R"({}
select s.SeriesID, s.SeriesTitle, s.IsDeleted, s.Flags, l.BookID, l.OrdNum
{}
join Series_List l on l.BookID = b.BookID
join Series s on s.SeriesID = l.SeriesID
{}
)";
			const auto query = db.CreateQuery(std::format(queryText, with, queryClause.navigationFrom, where));
			for (query->Execute(); !query->Eof(); query->Next())
			{
				const auto id = query->Get<long long>(0);
				auto& item = m_series[id];
				if (!item)
					item = DatabaseUtil::CreateSimpleListItem(*query);

				auto& book = m_books[query->Get<long long>(4)];
				assert(std::get<0>(book));
				std::get<1>(book).emplace(id);
				addFlag(*std::get<0>(book), item->GetFlags());
				if (std::get<0>(book)->GetData(BookItem::Column::Series).isEmpty())
					std::get<0>(book)->SetData(item->GetData(DataItem::Column::Title), BookItem::Column::Series);
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
			const auto query = db.CreateQuery(std::format(queryText, with, queryClause.navigationFrom, where));
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto id = query->Get<long long>(0);
				auto& item = m_authors[id];
				if (!item)
					item = DatabaseUtil::CreateFullAuthorItem(*query);

				auto& book = m_books[query->Get<long long>(6)];
				assert(std::get<0>(book));
				Add(std::get<2>(book), static_cast<long long&&>(id), query->Get<int>(7));
				addFlag(*std::get<0>(book), item->GetFlags());
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
			const auto query = db.CreateQuery(std::format(queryText, with, queryClause.navigationFrom, where));
			for (query->Execute(); !query->Eof(); query->Next())
			{
				QString id = query->Get<const char*>(0);
				auto& item = m_genres[id];
				if (!item)
					item = DatabaseUtil::CreateGenreItem(*query);

				auto& book = m_books[query->Get<long long>(5)];
				assert(std::get<0>(book));
				Add(std::get<3>(book), std::move(id), query->Get<int>(6));
				addFlag(*std::get<0>(book), item->GetFlags());
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
			const auto query = db.CreateQuery(std::format(queryText, with, queryClause.navigationFrom, where));
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto& book = m_books[query->Get<long long>(1)];
				assert(std::get<0>(book));
				addFlag(*std::get<0>(book), static_cast<IDataItem::Flags>(query->Get<int>(0)));
			}
		}

		for (auto& [book, seriesId, authorIds, genreIds] : m_books | std::views::values)
		{
			assert(!authorIds.second.empty());
			const auto& author = m_authors.find(authorIds.second.cbegin()->second)->second;
			auto authorStr = GetAuthorFull(*author);
			book->SetData(std::move(authorStr), BookItem::Column::AuthorFull);
		}

		for (const auto& author : m_authors | std::views::values)
			author->Reduce();

		for (auto& [book, seriesId, authorIds, genreIds] : m_books | std::views::values)
		{
			book->SetData(std::size(authorIds.second) > 1 ? Join(m_authors, authorIds.second | std::views::values) : book->GetRawData(BookItem::Column::AuthorFull), BookItem::Column::Author);
			book->SetData(Join(m_genres, genreIds.second | std::views::values), BookItem::Column::Genre);
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
