#include <set>
#include <shared_mutex>
#include <unordered_set>

#include <QAbstractItemModel>
#include <QPointer>
#include <QTimer>

#include "fnd/FindPair.h"
#include "fnd/observable.h"

#include "constants/ProductConstant.h"

#include "controllers/ModelControllers/BooksViewType.h"
#include "controllers/ModelControllers/NavigationSource.h"

#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "models/Book.h"
#include "models/BookRole.h"

#include "util/executor.h"
#include "util/Settings.h"

#include "BooksModelControllerObserver.h"
#include "BooksModelController.h"

namespace HomeCompa::Flibrary {

namespace {

using Role = BookRole;

constexpr auto QUERY =
"select b.BookID, b.Title, coalesce(b.SeqNumber, -1), b.UpdateDate, b.LibRate, b.Lang, b.Folder, b.FileName || b.Ext, b.IsDeleted "
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

std::unordered_set<QString> g_folders;  // NOLINT(clang-diagnostic-exit-time-destructors)
std::shared_mutex g_guardFolders;       // NOLINT(clang-diagnostic-exit-time-destructors)

bool UserSettingsEmpty()
{
	std::shared_lock r(g_guardFolders);
	return g_folders.empty();
}

void CollectUserSettings(Settings & settings)
{
	if (!UserSettingsEmpty())
		return;

	auto folders = settings.GetGroups();
	std::unordered_set<QString> foldersSet { std::make_move_iterator(std::begin(folders)), std::make_move_iterator(std::end(folders)) };

	std::unique_lock w(g_guardFolders);
	foldersSet.swap(g_folders);
}

bool CheckFileObsolete(Settings & settings, Book & item)
{
	if (!settings.HasGroup(item.FileName))
		return true;

	bool keyRemoved = false;
	SettingsGroup fileGroup(settings, item.FileName);
	if (settings.HasKey(Constant::IS_DELETED))
	{
		if (const bool isDeleted = settings.Get(Constant::IS_DELETED, item.IsDeleted ? 1 : 0).toInt(); isDeleted != item.IsDeleted)
		{
			item.IsDeleted = isDeleted;
		}
		else
		{
			settings.Remove(Constant::IS_DELETED);
			keyRemoved = true;
		}
	}

	return keyRemoved && settings.GetKeys().isEmpty();
}

bool CheckFolderObsolete(Settings & settings, Book & item)
{
	if (!settings.HasGroup(item.Folder))
		return true;

	SettingsGroup folderGroup(settings, item.Folder);
	if (!CheckFileObsolete(settings, item))
		return false;

	settings.Remove(item.FileName);
	return settings.GetGroups().isEmpty();
}

std::set<QString> GetObsoleteSettings(Settings & settings, Books & items)
{
	std::set<QString> toRemove;
	std::shared_lock r(g_guardFolders);
	for (auto & item : items)
	{
		if (!(true
			&& g_folders.contains(item.Folder)
			&& CheckFolderObsolete(settings, item)
		))
			continue;

		settings.Remove(item.Folder);
		toRemove.insert(item.Folder);
	}

	return toRemove;
}

void CheckUserSettings(Settings & settings, Books & items)
{
	CollectUserSettings(settings);
	const auto toRemove = GetObsoleteSettings(settings, items);
	if (toRemove.empty())
		return;

	std::unique_lock w(g_guardFolders);
	for (const auto & folder : toRemove)
		g_folders.erase(folder);
}

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
	[[maybe_unused]] size_t index;
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

		auto & indexValue = index.emplace(id, std::size(items)).first->second;
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

		std::vector<QString> itemAuthors;
		itemAuthors.reserve(indexValue.authors.size());
		std::ranges::transform(indexValue.authors, std::back_inserter(itemAuthors), [&authors] (const long long int authorId)
		{
			assert(authors.contains(authorId));
			const auto & author = authors[authorId];

			auto itemAuthor = author.last;
			AppendAuthorName(itemAuthor, author.first, " ");
			AppendAuthorName(itemAuthor, author.middle, "");
			return itemAuthor;
		});
		std::ranges::sort(itemAuthors);
		for (const auto & itemAuthor : itemAuthors)
			AppendTitle(item.Author, itemAuthor, ", ");

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

	const auto correctSeqNumber = [] (int n) { return n ? n : std::numeric_limits<int>::max(); };
	std::ranges::sort(items, [&] (const Book & lhs, const Book & rhs)
	{
		if (lhs.Author != rhs.Author)
			return lhs.Author < rhs.Author;

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

	return data;
}

Books CreateItemsList(DB::Database & db, const NavigationSource navigationSource, const QString & navigationId)
{
	auto [books, index, authors, series, genres] = CreateItems(db, navigationSource, navigationId);
	return books;
}

void PostProcess(const QString & parent, const bool deleted, std::set<QString> & languages, Books & books, const size_t index)
{
	if (parent.isEmpty())
		return languages.clear();

	assert(index < books.size());
	auto & book = books[index];
	book.IsDeleted = deleted;
	for (const auto & lang : languages)
		book.Lang.append(lang).append(",");

	languages.clear();
};

Books CreateBookTreeForAuthor(Books & items, const Series & series)
{
	Books result;
	result.reserve(items.size() + series.size() + 1);

	QString parentSeries;
	size_t parentPosition = 0;
	bool deleted = true;
	std::set<QString> languages;

	for (auto & item : items)
	{
		if (parentSeries != item.SeriesTitle)
		{
			PostProcess(parentSeries, deleted, languages, result, parentPosition);
			deleted = true;
			parentSeries = item.SeriesTitle;
			parentPosition = std::size(result);
			if (!parentSeries.isEmpty())
			{
				auto & r = result.emplace_back();
				r.Title = parentSeries;
				r.IsDictionary = true;
			}
		}

		if (!item.IsDeleted)
			deleted = false;

		auto & r = result.emplace_back(std::move(item));
		if (!parentSeries.isEmpty())
		{
			r.TreeLevel = 1;
			r.ParentId = parentPosition;
			languages.insert(r.Lang);
		}
	}

	PostProcess(parentSeries, deleted, languages, result, parentPosition);

	return result;
}

Books CreateBookTree(Books & items, const Index & index, const Authors & authorsIndex, const Series & seriesIndex)
{
	Books result;
	result.reserve(items.size() + authorsIndex.size() + seriesIndex.size() + 1);

	QString author;
	size_t authorPosition = 0;
	bool authorDeleted = true;
	std::set<QString> authorLang;

	QString series;
	size_t seriesPosition = 0;
	bool seriesDeleted = true;
	std::set<QString> seriesLang;

	for (auto & item : items)
	{
		if (author != item.Author)
		{
			PostProcess(author, authorDeleted, authorLang, result, authorPosition);
			authorDeleted = true;
			author = item.Author;
			authorPosition = std::size(result);
			if (!author.isEmpty())
			{
				auto & r = result.emplace_back();
				const auto it = index.find(item.Id);
				assert(it != index.end());
				std::vector<QString> authors;
				authors.reserve(it->second.authors.size());
				std::ranges::transform(it->second.authors, std::back_inserter(authors), [&authorsIndex] (const long long int authorId)
				{
					const auto authorIt = authorsIndex.find(authorId);
					assert(authorIt != authorsIndex.end());
					auto author = authorIt->second.last;
					AppendTitle(author, authorIt->second.first, " ");
					AppendTitle(author, authorIt->second.middle, " ");
					return author;
				});
				std::ranges::sort(authors);
				for (const auto & a : authors)
					AppendTitle(r.Title, a, ", ");

				r.IsDictionary = true;
			}
			if (!series.isEmpty())
				series = "this could be your ad";
		}

		if (series != item.SeriesTitle)
		{
			PostProcess(series, seriesDeleted, seriesLang, result, seriesPosition);
			seriesDeleted = true;
			series = item.SeriesTitle;
			seriesPosition = std::size(result);
			if (!series.isEmpty())
			{
				auto & r = result.emplace_back();
				r.Title = series;
				r.IsDictionary = true;
				r.TreeLevel = 1;
				if (!author.isEmpty())
					r.ParentId = authorPosition;
			}
		}

		auto & r = result.emplace_back(std::move(item));
		authorLang.insert(r.Lang);
		seriesLang.insert(r.Lang);
		if (!r.IsDeleted)
			authorDeleted = seriesDeleted = false;

		if (!series.isEmpty())
		{
			r.ParentId = seriesPosition;
			r.TreeLevel = 2;
		}

		else if (!author.isEmpty())
		{
			r.ParentId = authorPosition;
			r.TreeLevel = 1;
		}
	}

	PostProcess(author, authorDeleted, authorLang, result, authorPosition);
	PostProcess(series, seriesDeleted, seriesLang, result, seriesPosition);

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

	return CreateBookTree(books, index, authors, series);
}

using ItemsCreator = Books(*)(DB::Database &, NavigationSource, const QString &);
constexpr std::pair<BooksViewType, ItemsCreator> g_itemCreators[]
{
	{ BooksViewType::List, &CreateItemsList },
	{ BooksViewType::Tree, &CreateItemsTree },
};

}

QAbstractItemModel * CreateBooksListModel(Books & items, QObject * parent = nullptr);
QAbstractItemModel * CreateBooksTreeModel(Books & items, QObject * parent = nullptr);
using ModelCreator = QAbstractItemModel * (*)(Books & items, QObject * parent);
constexpr std::pair<BooksViewType, ModelCreator> g_modelCreators[]
{
	{ BooksViewType::List, &CreateBooksListModel },
	{ BooksViewType::Tree, &CreateBooksTreeModel },
};

struct BooksModelController::Impl
	: virtual Observable<BooksModelControllerObserver>
{
	Books books;
	QTimer setNavigationIdTimer;
	const BooksViewType booksViewType;
	NavigationSource navigationSource{ NavigationSource::Undefined };
	QString navigationId;

	Impl(BooksModelController & self, Util::Executor & executor, DB::Database & db, const BooksViewType booksViewType_)
		: booksViewType(booksViewType_)
		, m_self(self)
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
			Settings settings(Constant::COMPANY_ID, Constant::PRODUCT_ID);
			settings.BeginGroup(Constant::BOOKS);
			CheckUserSettings(settings, items);

			return[&, items = std::move(items)]() mutable
			{
				QPointer<QAbstractItemModel> model = m_self.GetCurrentModel();

				(void)model->setData({}, true, Role::ResetBegin);
				books = std::move(items);
				(void)model->setData({}, true, Role::ResetEnd);

				Perform(&BooksModelControllerObserver::HandleModelReset);
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

BooksModelController::~BooksModelController()
{
	if (auto * model = GetCurrentModel())
		(void)model->setData({}, QVariant::fromValue(To<BookModelObserver>()), Role::ObserverUnregister);
}

bool BooksModelController::RemoveAvailable(const long long id)
{
	return GetCurrentModel()->setData({}, id, Role::RemoveAvailable);
}

bool BooksModelController::RestoreAvailable(const long long id)
{
	return GetCurrentModel()->setData({}, id, Role::RestoreAvailable);
}

bool BooksModelController::AllSelected()
{
	return GetCurrentModel()->data({}, Role::AllSelected).toBool();
}

bool BooksModelController::HasSelected()
{
	return GetCurrentModel()->data({}, Role::HasSelected).toBool();
}

bool BooksModelController::SelectAll()
{
	return GetCurrentModel()->setData({}, true, Role::SelectAll);
}

bool BooksModelController::DeselectAll()
{
	return GetCurrentModel()->setData({}, false, Role::SelectAll);
}

bool BooksModelController::InvertSelection()
{
	return GetCurrentModel()->setData({}, {}, Role::InvertSelection);
}

void BooksModelController::Remove(long long id)
{
	(void)GetCurrentModel()->setData({}, id, Role::Remove);
}

void BooksModelController::Restore(long long id)
{
	(void)GetCurrentModel()->setData({}, id, Role::Restore);
}

void BooksModelController::Save(QString path, long long id)
{
	(void)GetCurrentModel()->setData({}, QVariant::fromValue(SaveSettings{std::move(path), id}), Role::Save);
}

void BooksModelController::SetNavigationState(NavigationSource navigationSource, const QString & navigationId)
{
	m_impl->navigationSource = navigationSource;
	m_impl->navigationId = navigationId;
	m_impl->setNavigationIdTimer.start();
}

void BooksModelController::RegisterObserver(BooksModelControllerObserver * observer)
{
	m_impl->Register(observer);
}

void BooksModelController::UnregisterObserver(BooksModelControllerObserver * observer)
{
	m_impl->Unregister(observer);
}

ModelController::Type BooksModelController::GetType() const noexcept
{
	return Type::Books;
}

QAbstractItemModel * BooksModelController::CreateModel()
{
	if (auto * model = GetCurrentModel())
		(void)model->setData({}, QVariant::fromValue(To<BookModelObserver>()), Role::ObserverUnregister);

	auto * model = FindSecond(g_modelCreators, m_impl->booksViewType)(m_impl->books, nullptr);
	(void)model->setData({}, QVariant::fromValue(To<BookModelObserver>()), Role::ObserverRegister);

	return model;
}

bool BooksModelController::SetCurrentIndex(const int index)
{
	if (index < 0)
		return false;

	assert(index < static_cast<int>(std::size(m_impl->books)));
	const auto & book = m_impl->books[index];
	if (!book.IsDictionary)
		m_impl->Perform(&BooksModelControllerObserver::HandleBookChanged, book.Folder.toStdString(), book.FileName.toStdString());

	return ModelController::SetCurrentIndex(index);
}

void BooksModelController::HandleBookRemoved(const Book & book)
{
	std::unique_lock w(g_guardFolders);
	g_folders.insert(book.Folder);
}

}
