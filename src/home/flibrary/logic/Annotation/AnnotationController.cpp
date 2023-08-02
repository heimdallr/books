#include "AnnotationController.h"

#include <plog/Log.h>

#include "shared/DatabaseUser.h"

using namespace HomeCompa::Flibrary;

namespace {
constexpr auto BOOK_QUERY = "select Title, SeriesID, SeqNumber, UpdateDate, LibRate, Folder, FileName || Ext, BookSize from Books where BookID = :id";
}

class AnnotationController::Impl final
	: DatabaseUser
{
public:
	explicit Impl(std::shared_ptr<ILogicFactory> logicFactory)
		: DatabaseUser(std::move(logicFactory))
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
	}

private:
	QString m_currentBookId;
	PropagateConstPtr<QTimer> m_extractInfoTimer { CreateTimer([&] { ExtractInfo(); }) };
};

AnnotationController::AnnotationController(std::shared_ptr<ILogicFactory> logicFactory)
	: m_impl(std::move(logicFactory))
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
