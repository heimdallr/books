#include "UpdateChecker.h"

#include <QDesktopServices>
#include <QTimer>

#include <plog/Log.h>

#include "fnd/ScopedCall.h"

#include "interface/constants/Localization.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/ui/IUiFactory.h"

#include "network/rest/api/github/Release.h"
#include "network/rest/api/github/Requester.h"
#include "network/rest/connection/ConnectionFactory.h"

#include "util/IExecutor.h"
#include "util/ISettings.h"

#include "config/version.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa::RestAPI;
using namespace Github;

namespace {
constexpr auto DISCARDED_UPDATE_KEY = "ui/Update/SkippedVersion";

constexpr auto CONTEXT    =                   "UpdateChecker";
constexpr auto DOWNLOAD   = QT_TRANSLATE_NOOP("UpdateChecker", "Download");
constexpr auto VISIT_HOME = QT_TRANSLATE_NOOP("UpdateChecker", "Visit download page");
constexpr auto SKIP       = QT_TRANSLATE_NOOP("UpdateChecker", "Skip this version");
constexpr auto CANCEL     = QT_TRANSLATE_NOOP("UpdateChecker", "Cancel");
constexpr auto RELEASED   = QT_TRANSLATE_NOOP("UpdateChecker", "%1 released!");
TR_DEF
}

struct UpdateChecker::Impl final : virtual IClient
{
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	std::shared_ptr<const ILogicFactory> logicFactory;
	std::shared_ptr<const IUiFactory> uiFactory;
	Callback callback;
	Release releaseInfo;

	Impl(std::shared_ptr<ISettings> settings
		, std::shared_ptr<const ILogicFactory> logicFactory
		, std::shared_ptr<const IUiFactory> uiFactory
	)
		: settings(std::move(settings))
		, logicFactory(std::move(logicFactory))
		, uiFactory(std::move(uiFactory))
	{
	}

	void HandleLatestRelease(const Release & release) override
	{
		releaseInfo = release;
	}

	bool IsLatestReleaseNewer()
	{
		const auto nameSplitted = releaseInfo.name.split(' ', Qt::SkipEmptyParts);
		if (nameSplitted.size() != 2)
			return false;

		if (settings->Get(DISCARDED_UPDATE_KEY, -1) == releaseInfo.id)
			return false;

		std::vector<int> latestVersion;
		if (!std::ranges::all_of(nameSplitted.back().split('.', Qt::SkipEmptyParts), [&] (const QString & item)
		{
			bool ok = false;
			latestVersion.push_back(item.toInt(&ok));
			return ok;
		}))
			return false;

		std::vector<int> currentVersion;
		std::ranges::transform(QString(PRODUCT_VERSION).split('.', Qt::SkipEmptyParts), std::back_inserter(currentVersion), [] (const QString & item)
		{
			bool ok = false;
			const auto value = item.toInt(&ok);
			assert(ok);
			return value;
		});

		if (latestVersion.size() != currentVersion.size())
			return false;

		return std::ranges::lexicographical_compare(currentVersion, latestVersion);
	}

	void ShowUpdateMessage()
	{
		ScopedCall sendCallback([&]{ callback(); });

		std::vector<std::pair<QMessageBox::ButtonRole, QString>> buttons
		{
			{ QMessageBox::AcceptRole, Tr(DOWNLOAD) },
			{ QMessageBox::DestructiveRole, Tr(VISIT_HOME) },
			{ QMessageBox::ActionRole, Tr(SKIP) },
			{ QMessageBox::RejectRole, Tr(CANCEL) },
		};
		if (releaseInfo.assets.empty() || releaseInfo.assets.front().browser_download_url.isEmpty())
			buttons.erase(buttons.begin());

		const auto defaultRole = buttons.front().first;

		switch (uiFactory->ShowCustomDialog(Loc::Tr(Loc::Ctx::COMMON, Loc::WARNING), Tr(RELEASED).arg(releaseInfo.name), buttons, defaultRole))  // NOLINT(clang-diagnostic-switch-enum)
		{
			case QMessageBox::AcceptRole:
				return (void)QDesktopServices::openUrl(releaseInfo.assets.front().browser_download_url);
			case QMessageBox::DestructiveRole:
				return (void)QDesktopServices::openUrl(releaseInfo.html_url);
			case QMessageBox::ActionRole:
				return settings->Set(DISCARDED_UPDATE_KEY, releaseInfo.id);
			case QMessageBox::RejectRole:
			case QMessageBox::NoRole:
				return;
			default:
				assert(false && "unexpected case");
		}
	}

	void Download()
	{
//		auto downloader = std::make_shared<Network::Downloader>();
//		auto file = std::make_shared<QFile>("T:/flibrary_setup_0.9.4.exe");
//		file->open(QIODevice::WriteOnly);
//		downloader->Download("https://github.com/heimdallr/books/releases/download/Release/0/0.9.4/flibrary_setup_0.9.4.exe", *file, [downloader, file] (int /*code*/, const QString & /*error*/) mutable
//		{
//			file->close();
//			file.reset();
//			QTimer::singleShot(0, [downloader = std::move(downloader)] () mutable
//			{
//				downloader.reset();
//			});
//		}, [&, progressItem = std::shared_ptr<IProgressController::IProgressItem>{}] (const int64_t bytesReceived, const int64_t bytesTotal) mutable
//		{
//			if (!progressItem)
//				progressItem = m_progressController->Add(bytesTotal);
//			progressItem->Increment(bytesReceived);
//			PLOGD << bytesReceived << ", " << bytesTotal;
//		});

	}
};

UpdateChecker::UpdateChecker(std::shared_ptr<ISettings> settings
	, std::shared_ptr<ILogicFactory> logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
)
	: m_impl(std::make_shared<Impl>(std::move(settings)
		, std::move(logicFactory)
		, std::move(uiFactory)
	))
{
	PLOGD << "UpdateChecker created";
}

UpdateChecker::~UpdateChecker()
{
	PLOGD << "UpdateChecker destroyed";
}

void UpdateChecker::CheckForUpdate(Callback callback)
{
	m_impl->callback = std::move(callback);
	std::shared_ptr executor = m_impl->logicFactory->GetExecutor();
	(*executor)({"Check for FLibrary updates", [&, executor]() mutable
	{
		Requester requester { CreateQtConnection("https://api.github.com") };
		requester.GetLatestRelease(m_impl, "heimdallr", "books");
		const auto needUpdate = m_impl->IsLatestReleaseNewer();

		return [&, executor = std::move(executor), needUpdate] (size_t) mutable
		{
			if (needUpdate)
				QTimer::singleShot(0, [&] { m_impl->ShowUpdateMessage(); });
			else
				m_impl->callback();

			executor.reset();
		};
	}});

}
