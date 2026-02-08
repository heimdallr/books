#include "LogController.h"

#include <ranges>

#include <QAbstractItemModel>
#include <QDir>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/Localization.h"
#include "interface/constants/PLogSeverityLocalization.h"
#include "interface/logic/LogModelRole.h"

#include "model/LogModel.h"

#include "log.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto CONTEXT    = "CollectionStatistics";
constexpr auto STATISTICS = QT_TRANSLATE_NOOP("CollectionStatistics", "Collection statistics:");
constexpr auto NAME       = QT_TRANSLATE_NOOP("CollectionStatistics", "Name: %1");
constexpr auto FOLDER     = QT_TRANSLATE_NOOP("CollectionStatistics", "Folder: %1");
constexpr auto DATABASE   = QT_TRANSLATE_NOOP("CollectionStatistics", "Database: %1");

TR_DEF

}

struct LogController::Impl
{
	std::unique_ptr<QAbstractItemModel>        model { CreateLogModel() };
	std::shared_ptr<const IDatabaseUser>       databaseUser;
	std::shared_ptr<const ICollectionProvider> collectionProvider;

	explicit Impl(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const ICollectionProvider> collectionProvider)
		: databaseUser { std::move(databaseUser) }
		, collectionProvider { std::move(collectionProvider) }
	{
	}
};

LogController::LogController(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const ICollectionProvider> collectionProvider)
	: m_impl(std::move(databaseUser), std::move(collectionProvider))
{
}

LogController::~LogController() = default;

QAbstractItemModel* LogController::GetModel() const noexcept
{
	return m_impl->model.get();
}

void LogController::Clear()
{
	m_impl->model->setData({}, {}, LogModelRole::Clear);
}

std::vector<const char*> LogController::GetSeverities() const
{
	std::vector<const char*> result;
	result.reserve(std::size(SEVERITIES));
	std::ranges::copy(SEVERITIES | std::views::keys, std::back_inserter(result));
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
	if (!m_impl->collectionProvider->ActiveCollectionExists())
		return;

	m_impl->databaseUser->Execute({ "Get collection statistics", [&] {
									   static constexpr auto dbStatQueryText =
										   "select '%1', count(42) from Authors union all "
										   "select '%2', count(42) from Series union all "
										   "select '%3', count(42) from Keywords union all "
										   "select '%4', count(42) from Books union all "
										   "select '%5', count(42) from Books b left join Books_User bu on bu.BookID = b.BookID where coalesce(bu.IsDeleted, b.IsDeleted, 0) != 0";

									   const auto& collection = m_impl->collectionProvider->GetActiveCollection();
									   QStringList stats;
									   stats << Tr(STATISTICS) << Tr(NAME).arg(collection.name) << Tr(FOLDER).arg(QDir::toNativeSeparators(collection.GetFolder()))
											 << Tr(DATABASE).arg(QDir::toNativeSeparators(collection.GetDatabase()));

									   const auto bookQuery = m_impl->databaseUser->Database()->CreateQuery(QString(dbStatQueryText)
		                                                                                                        .arg(
																													QT_TRANSLATE_NOOP("CollectionStatistics", "Authors:"),
																													QT_TRANSLATE_NOOP("CollectionStatistics", "Series:"),
																													QT_TRANSLATE_NOOP("CollectionStatistics", "Keywords:"),
																													QT_TRANSLATE_NOOP("CollectionStatistics", "Books:"),
																													QT_TRANSLATE_NOOP("CollectionStatistics", "Deleted books:")
																												)
		                                                                                                        .toStdString());
									   for (bookQuery->Execute(); !bookQuery->Eof(); bookQuery->Next())
									   {
										   [[maybe_unused]] const auto* name       = bookQuery->Get<const char*>(0);
										   [[maybe_unused]] const auto  translated = Loc::Tr("CollectionStatistics", bookQuery->Get<const char*>(0));
										   stats << QString("%1 %2").arg(translated).arg(bookQuery->Get<long long>(1));
									   }

									   return [stats = stats.join("\n")](size_t) {
										   PLOGI << stats;
									   };
								   } });
}

void LogController::TestColors() const
{
	std::ranges::for_each(SEVERITIES | std::views::keys, [n = 0](const auto& item) mutable {
		PLOG(static_cast<plog::Severity>(n)) << Loc::Tr(Loc::Ctx::LOGGING, item);
		++n;
	});
}
