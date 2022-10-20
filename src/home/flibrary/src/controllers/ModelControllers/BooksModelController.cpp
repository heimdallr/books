#include <QAbstractItemModel>
#include <QPointer>
#include <QTimer>

#include "fnd/FindPair.h"

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
"from Books b "
"join Author_List al on al.BookID = b.BookID "
"join Authors a on a.AuthorID = al.AuthorID "
"join Genre_List gl on gl.BookID = b.BookID "
"join Genres g on g.GenreCode = gl.GenreCode "
"left join Series s on s.SeriesID = b.SeriesID "
;

constexpr auto WHERE_AUTHOR = "where a.AuthorID = :id";
constexpr auto WHERE_SERIES = "where b.SeriesID = :id";

constexpr std::pair<const char *, const char *> g_joins[]
{
	{ "Authors", WHERE_AUTHOR },
	{ "Series", WHERE_SERIES },
};

void AppendTitle(QString & title, std::string_view str, std::string_view separator)
{
	if (!str.empty())
		title.append(separator.data()).append(str.data());
}

Books CreateItems(DB::Database & db, const std::string & navigationType, int navigationId)
{
	std::map<long long int, size_t> index;

	Books items;
	const auto query = db.CreateQuery(std::string(QUERY) + FindSecond(g_joins, navigationType.data(), PszComparer{}));
	[[maybe_unused]] const auto result = query->Bind(":id", navigationId);
	assert(result == 0);

	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto id = query->Get<long long int>(0);
		const auto it = index.find(id);
		if (it != index.end())
		{
			AppendTitle(items[it->second].GenreAlias, query->Get<const char *>(12), " / ");
			continue;
		}

		index.emplace(id, std::size(items));
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
		item.Author = query->Get<const char *>(9);
		AppendTitle(item.Author, query->Get<const char *>(10), " ");
		AppendTitle(item.Author, query->Get<const char *>(11), " ");
		item.GenreAlias = query->Get<const char *>(12);
		item.SeriesTitle = query->Get<const char *>(13);
	}

	return items;
}

}

QAbstractItemModel * CreateBooksModel(Books & items, QObject * parent = nullptr);

struct BooksModelController::Impl
{
	Books books;
	QTimer setNavigationIdTimer;
	QString navigationType;
	int navigationId { -1 };

	Impl(BooksModelController & self, Util::Executor & executor, DB::Database & db)
		: m_self(self)
		, m_executor(executor)
		, m_db(db)
	{
		setNavigationIdTimer.setSingleShot(true);
		setNavigationIdTimer.setInterval(std::chrono::milliseconds(250));
		connect(&setNavigationIdTimer, &QTimer::timeout, [&]
		{
			if (m_navigationId == navigationId)
				return;

			m_navigationId = navigationId;
			UpdateItems();
		});
	}

	void UpdateItems()
	{
		QPointer<QAbstractItemModel> model = m_self.GetCurrentModel();
		m_executor([&, model = std::move(model), navigationType = navigationType.toStdString(), navigationId = m_navigationId]() mutable
		{
			auto items = CreateItems(m_db, navigationType, navigationId);
			return[&, items = std::move(items), model = std::move(model)]() mutable
			{
				if (model.isNull())
					return;

				(void)model->setData({}, true, Role::ResetBegin);
				books = std::move(items);
				(void)model->setData({}, true, Role::ResetEnd);
			};
		}, 2);
	}

private:
	BooksModelController & m_self;
	Util::Executor & m_executor;
	DB::Database & m_db;
	int m_navigationId { -1 };
};

BooksModelController::BooksModelController(Util::Executor & executor, DB::Database & db)
	: ModelController()
	, m_impl(*this, executor, db)
{
}

BooksModelController::~BooksModelController() = default;

void BooksModelController::SetNavigationId(const QString & navigationType, int navigationId)
{
	m_impl->navigationType = navigationType;
	m_impl->navigationId = navigationId;
	m_impl->setNavigationIdTimer.start();
}

ModelController::Type BooksModelController::GetType() const noexcept
{
	return Type::Books;
}

QAbstractItemModel * BooksModelController::GetModelImpl(const QString & /*modelType*/)
{
	return CreateBooksModel(m_impl->books);
}

}
