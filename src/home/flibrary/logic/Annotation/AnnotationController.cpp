#include "AnnotationController.h"

#include <plog/Log.h>

#include "shared/DatabaseUser.h"

using namespace HomeCompa::Flibrary;

namespace {
constexpr auto BOOK_QUERY = "select Title, SeriesID, SeqNumber, UpdateDate, LibRate, Folder, FileName || Ext, BookSize from Books where BookID = :id";
}

class AnnotationController::Impl final
{
public:
	explicit Impl(std::shared_ptr<DatabaseUser> databaseUser)
		: m_databaseUser(std::move(databaseUser))
	{
	}

public:
	void SetCurrentBookId(QString bookId)
	{
		m_currentBookId = std::move(bookId);
		m_extractInfoTimer->start();
	}

private:
	void ExtractInfo()
	{
		m_databaseUser->Execute({ "Get book info", [&]
		{
			const auto db = m_databaseUser->Database();
			return [] (size_t)
			{
			};
		} }, 3);
	}

private:
	QString m_currentBookId;
	PropagateConstPtr<DatabaseUser, std::shared_ptr> m_databaseUser;
	PropagateConstPtr<QTimer> m_extractInfoTimer { DatabaseUser::CreateTimer([&] { ExtractInfo(); }) };
};

AnnotationController::AnnotationController(std::shared_ptr<DatabaseUser> databaseUser)
	: m_impl(std::move(databaseUser))
{
	PLOGD << "AnnotationController created";
}
AnnotationController::~AnnotationController()
{
	PLOGD << "AnnotationController destroyed";
}

void AnnotationController::SetCurrentBookId(QString bookId)
{
	m_impl->SetCurrentBookId(std::move(bookId));
}
