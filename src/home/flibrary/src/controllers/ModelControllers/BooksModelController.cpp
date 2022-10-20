#include <QAbstractItemModel>
#include <QPointer>
#include <QTimer>

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
	Books books;
	QTimer setNavigationIdTimer;
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
		m_executor([&, model = std::move(model)]() mutable
		{
			auto items = CreateItems(m_db, m_navigationId);
			return[&, items = std::move(items), model = std::move(model)]() mutable
			{
				if (model.isNull())
					return;

				(void)model->setData({}, true, Role::ResetBegin);
				books = std::move(items);
				(void)model->setData({}, true, Role::ResetEnd);
			};
		});
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

void BooksModelController::SetNavigationId(int navigationId)
{
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
