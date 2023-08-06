#include "LogController.h"

#include <QAbstractItemModel>

#include <plog/Log.h>
#include <plog/Severity.h>

#include "interface/constants/Localization.h"
#include "model/LogModel.h"
#include "shared/DatabaseUser.h"

using namespace HomeCompa::Flibrary;

namespace {

static_assert(plog::Severity::none == 0);
static_assert(plog::Severity::fatal == 1);
static_assert(plog::Severity::error == 2);
static_assert(plog::Severity::warning == 3);
static_assert(plog::Severity::info == 4);
static_assert(plog::Severity::debug == 5);
static_assert(plog::Severity::verbose == 6);

constexpr auto NONE = QT_TRANSLATE_NOOP("Logging", "NONE");
constexpr auto FATAL = QT_TRANSLATE_NOOP("Logging", "FATAL");
constexpr auto ERROR = QT_TRANSLATE_NOOP("Logging", "ERROR");
constexpr auto WARN = QT_TRANSLATE_NOOP("Logging", "WARN");
constexpr auto INFO = QT_TRANSLATE_NOOP("Logging", "INFO");
constexpr auto DEBUG = QT_TRANSLATE_NOOP("Logging", "DEBUG");
constexpr auto VERB = QT_TRANSLATE_NOOP("Logging", "VERB");

constexpr const char * SEVERITIES[]
{
	NONE,
	FATAL,
	ERROR,
	WARN,
	INFO,
	DEBUG,
	VERB,
};

}

struct LogController::Impl
{
	std::unique_ptr<QAbstractItemModel> model { CreateLogModel() };
	PropagateConstPtr<DatabaseUser, std::shared_ptr> databaseUser;
	explicit Impl(std::shared_ptr<DatabaseUser> databaseUser)
		: databaseUser(std::move(databaseUser))
	{
	}
};

LogController::LogController(std::shared_ptr<DatabaseUser> databaseUser)
	: m_impl(std::move(databaseUser))
{
}

LogController::~LogController() = default;

QAbstractItemModel * LogController::GetModel() const noexcept
{
	return m_impl->model.get();
}

void LogController::Clear()
{
	m_impl->model->setData({}, {}, LogModelRole::Clear);
}

std::vector<const char *> LogController::GetSeverities() const
{
	return { std::cbegin(SEVERITIES), std::cend(SEVERITIES) };
}

int LogController::GetSeverity() const
{
	return m_impl->model->data({}, LogModelRole::Severity).toInt();
}

void LogController::SetSeverity(const int value)
{
	m_impl->model->setData({}, value, LogModelRole::Severity);
}

void LogController::ShowCollectionStatistics() const
{
	m_impl->databaseUser->Execute({ "Get collection statistics", [&]
	{
		static constexpr auto dbStatQueryText =
			"select '%1', count(42) from Authors union all "
			"select '%2', count(42) from Series union all "
			"select '%3', count(42) from Books union all "
			"select '%4', count(42) from Books b left join Books_User bu on bu.BookID = b.BookID where coalesce(bu.IsDeleted, b.IsDeleted, 0) != 0"
			;

		QStringList stats;
		stats << Loc::Tr("CollectionStatistics", "Collection statistics:");
		const auto bookQuery = m_impl->databaseUser->Database()->CreateQuery(QString(dbStatQueryText).arg
		(
			  QT_TRANSLATE_NOOP("CollectionStatistics", "Authors:")
			, QT_TRANSLATE_NOOP("CollectionStatistics", "Series:")
			, QT_TRANSLATE_NOOP("CollectionStatistics", "Books:")
			, QT_TRANSLATE_NOOP("CollectionStatistics", "Deleted books:")
		).toStdString());
		for (bookQuery->Execute(); !bookQuery->Eof(); bookQuery->Next())
		{
			[[maybe_unused]] const auto * name = bookQuery->Get<const char *>(0);
			[[maybe_unused]] const auto translated = Loc::Tr("CollectionStatistics", bookQuery->Get<const char *>(0));
			stats << QString("%1 %2").arg(translated).arg(bookQuery->Get<long long>(1));
		}

		return[stats = stats.join("\n")] (size_t)
		{
			PLOGI << std::endl << stats;
		};
	} });
}
