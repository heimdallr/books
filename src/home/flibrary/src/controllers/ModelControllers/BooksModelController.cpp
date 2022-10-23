#include <set>
#include <ranges>

#include <QAbstractItemModel>
#include <QPointer>
#include <QTimer>

#include "fnd/FindPair.h"

#include "controllers/ModelControllers/BooksViewType.h"
#include "controllers/ModelControllers/NavigationSource.h"

#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "models/Book.h"
#include "models/RoleBase.h"

#include "util/executor.h"

#include "BooksModelController.h"

namespace HomeCompa::Flibrary {

namespace {

using Role = RoleBase;

constexpr auto QUERY =
"select b.BookID, b.Title, coalesce(b.SeqNumber, -1), b.UpdateDate, b.LibRate, b.Lang, b.Folder, b.FileName, b.IsDeleted "
", a.LastName, a.FirstName, a.MiddleName "
", g.GenreAlias, s.SeriesTitle "
", a.AuthorID, b.SeriesID, g.GenreCode "
"from Books b "
"join Author_List al on al.BookID = b.BookID "
"join Authors a on a.AuthorID = al.AuthorID "
"join Genre_List gl on gl.BookID = b.BookID "
"join Genres g on g.GenreCode = gl.GenreCode "
"left join Series s on s.SeriesID = b.SeriesID "
;

constexpr auto WHERE_AUTHOR = "where a.AuthorID = :id";
constexpr auto WHERE_SERIES = "where b.SeriesID = :id";
constexpr auto WHERE_GENRE = "where g.GenreCode = :id";

using Binder = int(*)(DB::Query &, const QString &);
int BindInt(DB::Query & query, const QString & id)
{
	return query.Bind(":id", id.toInt());
}

int BindString(DB::Query & query, const QString & id)
{
	return query.Bind(":id", id.toStdString());
}

constexpr std::pair<NavigationSource, std::pair<const char *, Binder>> g_joins[]
{
	{ NavigationSource::Authors, { WHERE_AUTHOR, &BindInt    } },
	{ NavigationSource::Series , { WHERE_SERIES, &BindInt    } },
	{ NavigationSource::Genres , { WHERE_GENRE , &BindString } },
};

void AppendTitle(QString & title, const QString & str, std::string_view separator)
{
	if (!str.isEmpty())
		title.append(title.isEmpty() ? "" : separator.data()).append(str);
}

void AppendAuthorName(QString & title, const QString & str, std::string_view separator)
{
	if (!str.isEmpty())
		AppendTitle(title, str.mid(0, 1) + ".", separator);
}

struct IndexValue
{
	IndexValue(size_t index_)
		: index(index_)
	{
	}

	size_t index;
	std::set<long long int> authors;
	std::set<QString> genres;
};

using Index = std::unordered_map<long long int, IndexValue>;

struct Author
{
	QString last;
	QString first;
	QString middle;
};
using Authors = std::unordered_map<long long int, Author>;
using Series = std::unordered_map<long long int, QString>;
using Genres = std::unordered_map<QString, QString>;

struct Data
{
	Books books;
	Index index;
	Authors authors;
	Series series;
	Genres genres;
};

Data CreateItems(DB::Database & db, const NavigationSource navigationSource, const QString & navigationId)
{
	if (navigationSource == NavigationSource::Undefined)
		return {};

	Data data;
	auto & [items, index, authors, series, genres] = data;

	const auto [whereClause, bind] = FindSecond(g_joins, navigationSource);
	const auto query = db.CreateQuery(std::string(QUERY) + whereClause);
	[[maybe_unused]] const auto result = bind(*query, navigationId);
	assert(result == 0);

	for (query->Execute(); !query->Eof(); query->Next())
	{
		authors.emplace(query->Get<long long int>(14), Author{ query->Get<const char *>(9), query->Get<const char *>(10), query->Get<const char *>(11) });
		series.emplace(query->Get<long long int>(15), query->Get<const char *>(13));
		genres.emplace(query->Get<const char *>(16), query->Get<const char *>(12));

		const auto id = query->Get<long long int>(0);
		const auto it = index.find(id);

		const auto updateIndexValue = [&] (IndexValue & value)
		{
			value.authors.emplace(query->Get<long long int>(14));
			value.genres.emplace(query->Get<const char *>(16));
		};

		if (it != index.end())
		{
			updateIndexValue(it->second);
			continue;
		}

		auto indexValue = index.emplace(id, std::size(items)).first->second;
		updateIndexValue(indexValue);

		auto & item = items.emplace_back();

		item.Id = id;
		item.Title = query->Get<const char *>(1);
		item.SeqNumber = query->Get<int>(2);
		item.UpdateDate = query->Get<const char *>(3);
		item.LibRate = query->Get<int>(4);
		item.Lang = query->Get<const char *>(5);
		item.Folder = query->Get<const char *>(6);
		item.FileName = query->Get<const char *>(7);
		item.IsDeleted = query->Get<int>(8) != 0;
		item.SeriesTitle = query->Get<const char *>(13);
	}

	for (auto & item : items)
	{
		const auto it = index.find(item.Id);
		assert(it != index.end());
		const auto & indexValue = it->second;

		for (const auto authorId : indexValue.authors)
		{
			assert(authors.contains(authorId));
			const auto & author = authors[authorId];

			auto itemAuthor = author.last;
			AppendAuthorName(itemAuthor, author.first, " ");
			AppendAuthorName(itemAuthor, author.middle, "");

			AppendTitle(item.Author, itemAuthor, ", ");
		}

		for (const auto & genreId : indexValue.genres)
		{
			assert(genres.contains(genreId));
			auto genre = genres[genreId];

			bool isNumber = false;
			(void)genre.toInt(&isNumber);
			if (isNumber)
				genre.clear();

			AppendTitle(item.GenreAlias, genre, " / ");
		}
	}

	return data;
}

Books CreateItemsList(DB::Database & db, const NavigationSource navigationSource, const QString & navigationId)
{
	auto [books, index, authors, series, genres] = CreateItems(db, navigationSource, navigationId);
	return books;
}

Books CreateBookTreeForAuthor(Books & items, const Series & series)
{
	const auto correctSeqNumber = [] (int n) { return n ? n : std::numeric_limits<int>::max(); };
	std::ranges::sort(items, [&] (const Book & lhs, const Book & rhs)
	{
		if (lhs.SeriesTitle == rhs.SeriesTitle)
			return
				  lhs.SeqNumber != rhs.SeqNumber   ? correctSeqNumber(lhs.SeqNumber) < correctSeqNumber(rhs.SeqNumber)
				: lhs.Title != rhs.Title           ? lhs.Title < rhs.Title
				: lhs.UpdateDate != rhs.UpdateDate ? lhs.UpdateDate < rhs.UpdateDate
				:									 lhs.Id < rhs.Id
				;

		if (!lhs.SeriesTitle.isEmpty() && !rhs.SeriesTitle.isEmpty())
			return lhs.SeriesTitle < rhs.SeriesTitle;

		return rhs.SeriesTitle.isEmpty();
	});


	Books result;
	result.reserve(items.size() + series.size() + 1);
	QString parentSeries;
	size_t parentId = 0;
	std::set<QString> langs;

	const auto processLangs = [&] ()
	{
		if (parentSeries.isEmpty())
			return langs.clear();

		for (const auto & lang : langs)
			result[parentId].Lang.append(lang).append(",");

		langs.clear();
	};

	for (auto & item : items)
	{
		if (parentSeries != item.SeriesTitle)
		{
			processLangs();
			parentSeries = item.SeriesTitle;
			parentId = std::size(result);
			if (!parentSeries.isEmpty())
			{
				auto & r = result.emplace_back();
				r.Title = parentSeries;
				r.IsDictionary = true;
			}
		}

		auto & r = result.emplace_back(std::move(item));
		if (!parentSeries.isEmpty())
		{
			r.ParentId = parentId;
			langs.insert(r.Lang);
		}
	}

	processLangs();

	return result;
}

Books CreateItemsTree(DB::Database & db, const NavigationSource navigationSource, const QString & navigationId)
{
	auto [books, index, authors, series, genres] = CreateItems(db, navigationSource, navigationId);

	switch (navigationSource)
	{
		case NavigationSource::Series: return books;
		case NavigationSource::Authors: return CreateBookTreeForAuthor(books, series);
		default:
			break;
	}

	return books;
}

using ItemsCreator = Books(*)(DB::Database &, NavigationSource, const QString &);
constexpr std::pair<BooksViewType, ItemsCreator> g_itemCreators[]
{
	{ BooksViewType::List, &CreateItemsList },
	{ BooksViewType::Tree, &CreateItemsTree },
};

}

QAbstractItemModel * CreateBooksModel(Books & items, QObject * parent = nullptr);

struct BooksModelController::Impl
{
	Books books;
	QTimer setNavigationIdTimer;
	NavigationSource navigationSource{ NavigationSource::Undefined };
	QString navigationId;

	Impl(BooksModelController & self, Util::Executor & executor, DB::Database & db, const BooksViewType booksViewType)
		: m_self(self)
		, m_executor(executor)
		, m_db(db)
		, m_itemsCreator(FindSecond(g_itemCreators, booksViewType))
	{
		setNavigationIdTimer.setSingleShot(true);
		setNavigationIdTimer.setInterval(std::chrono::milliseconds(250));
		connect(&setNavigationIdTimer, &QTimer::timeout, [&]
		{
			if (m_navigationId == navigationId && m_navigationSource == navigationSource)
				return;

			m_navigationSource = navigationSource;
			m_navigationId = navigationId;

			UpdateItems();
		});
	}

	void UpdateItems()
	{
		m_executor([&, navigationSource = m_navigationSource, navigationId = m_navigationId]
		{
			auto items = m_itemsCreator(m_db, navigationSource, navigationId);
			return[&, items = std::move(items)]() mutable
			{
				QPointer<QAbstractItemModel> model = m_self.GetCurrentModel();

				(void)model->setData({}, true, Role::ResetBegin);
				books = std::move(items);
				(void)model->setData({}, true, Role::ResetEnd);

				emit m_self.NavigationTypeChanged();
			};
		}, 2);
	}

private:
	BooksModelController & m_self;
	Util::Executor & m_executor;
	DB::Database & m_db;
	const ItemsCreator m_itemsCreator;
	NavigationSource m_navigationSource{ NavigationSource::Undefined };
	QString m_navigationId;
};

BooksModelController::BooksModelController(Util::Executor & executor, DB::Database & db, const BooksViewType booksViewType)
	: ModelController()
	, m_impl(*this, executor, db, booksViewType)
{
}

BooksModelController::~BooksModelController() = default;

void BooksModelController::SetNavigationState(NavigationSource navigationSource, const QString & navigationId)
{
	m_impl->navigationSource = navigationSource;
	m_impl->navigationId = navigationId;
	m_impl->setNavigationIdTimer.start();
}

ModelController::Type BooksModelController::GetType() const noexcept
{
	return Type::Books;
}

QAbstractItemModel * BooksModelController::CreateModel()
{
	return CreateBooksModel(m_impl->books);
}

}
