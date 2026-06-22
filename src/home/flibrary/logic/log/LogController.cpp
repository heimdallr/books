#include "LogController.h"

#include <ranges>

#include <QAbstractItemModel>
#include <QDir>

#include "interface/constants/PLogSeverityLocalization.h"
#include "interface/localization.h"
#include "interface/logic/LogModelRole.h"

#include "model/LogModel.h"

#include "log.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

}

struct LogController::Impl
{
	std::unique_ptr<QAbstractItemModel>        model { CreateLogModel() };
	std::shared_ptr<const IDatabaseUser>       databaseUser;
	std::shared_ptr<const ICollectionProvider> collectionProvider;

	Impl(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const ICollectionProvider> collectionProvider)
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

	m_impl->databaseUser->Execute({ "Get collection statistics", [this] {
									   return [stats = m_impl->collectionProvider->GetCollectionStatistics(*m_impl->databaseUser, true).join("\n")](size_t) {
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
