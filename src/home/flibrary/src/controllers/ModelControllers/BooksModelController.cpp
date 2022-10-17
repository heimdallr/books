#include <QAbstractItemModel>
#include <QTimer>

#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "models/Book.h"
#include "BooksModelController.h"

namespace HomeCompa::Flibrary {

namespace {

constexpr auto QUERY =
	"select b.BookID, b.Title "
	"from books b "
	"join Author_List al on al.BookID = b.BookID and al.AuthorID = :id"
	;

Books CreateItems(DB::Database & db, const int authorId)
{
	Books items;
	const auto query = db.CreateQuery(QUERY);
	[[maybe_unused]] const auto result = query->Bind(":id", authorId);
	assert(result == 0);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		items.emplace_back();
		items.back().Id = query->Get<long long int>(0);
		items.back().Title = query->Get<const char *>(1);
	}

	return items;
}

}

QAbstractItemModel * CreateBooksModel(Books & items, QObject * parent = nullptr);

struct BooksModelController::Impl
{
	QTimer setAuthorTimer;
	int authorId { -1 };

	Impl(BooksModelController & self, Executor & executor, DB::Database & db)
		: m_self(self)
		, m_executor(executor)
		, m_db(db)
	{
		setAuthorTimer.setSingleShot(true);
		setAuthorTimer.setInterval(std::chrono::milliseconds(250));
//		connect(&setAuthorTimer, &QTimer::timeout, [&]
//		{
//			if (m_authorId == authorId)
//				return;
//
//			m_authorId = authorId;
//			m_self.ResetModel(CreateBooksModel(m_db, m_authorId));
//			emit m_self.ModelChanged();
//		});
	}

private:
	BooksModelController & m_self;
	Executor & m_executor;
	DB::Database & m_db;
};

BooksModelController::BooksModelController(Executor & executor, DB::Database & db)
	: ModelController(CreateBooksModel(m_books))
	, m_impl(*this, executor, db)
{
}

BooksModelController::~BooksModelController() = default;

ModelController::Type BooksModelController::GetType() const noexcept
{
	return Type::Books;
}

void BooksModelController::SetAuthorId(int authorId)
{
	m_impl->authorId = authorId;
	m_impl->setAuthorTimer.start();
}

}
