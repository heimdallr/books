#include "LogController.h"

#include <ranges>

#include <QAbstractItemModel>

#include <plog/Log.h>

#include "interface/constants/Localization.h"
#include "interface/constants/PLogSeverityLocalization.h"
#include "interface/logic/LogModelRole.h"
#include "model/LogModel.h"
#include "shared/DatabaseUser.h"

using namespace HomeCompa::Flibrary;

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
	std::vector<const char *> result;
	result.reserve(std::size(SEVERITIES));
	std::ranges::transform(SEVERITIES, std::back_inserter(result), [] (const auto & item)
	{
		return item.first;
	});
	return result;
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
			PLOGI << stats;
		};
	} });
}

void LogController::TestColors() const
{
	std::ranges::for_each(SEVERITIES, [n = 0] (const auto & item) mutable
	{
		PLOG(static_cast<plog::Severity>(n)) << Loc::Tr(Loc::Ctx::LOGGING, item.first);
		++n;
	});
}
