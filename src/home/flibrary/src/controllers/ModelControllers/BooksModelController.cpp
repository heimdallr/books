#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QTimer>

#include "BooksModelController.h"

namespace HomeCompa::Flibrary {

QAbstractItemModel * CreateBooksModel(DB::Database & db, int authorId, QObject * parent = nullptr);

struct BooksModelController::Impl
{
	QTimer setAuthorTimer;
	int authorId { -1 };

	Impl(ModelController & self, DB::Database & db)
		: m_self(self)
		, m_db(db)
	{
		setAuthorTimer.setSingleShot(true);
		setAuthorTimer.setInterval(std::chrono::milliseconds(250));
		connect(&setAuthorTimer, &QTimer::timeout, [&]
		{
			if (m_authorId == authorId)
				return;

			m_authorId = authorId;
			m_self.ResetModel(CreateBooksModel(m_db, m_authorId));
			emit m_self.ModelChanged();
		});
	}

private:
	ModelController & m_self;
	DB::Database & m_db;
	int m_authorId { -1 };
};

BooksModelController::BooksModelController(DB::Database & db)
	: m_impl(*this, db)
{
	QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

BooksModelController::~BooksModelController()
{
	ResetModel();
}

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
