#include "UpdateChecker.h"

#include <QDesktopServices>
#include <QProcess>
#include <QTimer>

#include <plog/Log.h>

#include "interface/constants/Localization.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"
#include "interface/ui/IUiFactory.h"

#include "network/network/downloader.h"

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

constexpr auto CONTEXT          =                   "UpdateChecker";
constexpr auto DOWNLOAD         = QT_TRANSLATE_NOOP("UpdateChecker", "Download");
constexpr auto VISIT_HOME       = QT_TRANSLATE_NOOP("UpdateChecker", "Visit download page");
constexpr auto SKIP             = QT_TRANSLATE_NOOP("UpdateChecker", "Skip this version");
constexpr auto CANCEL           = QT_TRANSLATE_NOOP("UpdateChecker", "Cancel");
constexpr auto RELEASED         = QT_TRANSLATE_NOOP("UpdateChecker", "%1 released!");
constexpr auto INSTALLER_FOLDER = QT_TRANSLATE_NOOP("UpdateChecker", "Select folder for app installer");
constexpr auto START_INSTALLER  = QT_TRANSLATE_NOOP("UpdateChecker", "Run the installer?");
TR_DEF
}

class UpdateChecker::Impl final : virtual public IClient
{
public:
	Impl(std::shared_ptr<ISettings> settings
		, std::shared_ptr<const ILogicFactory> logicFactory
		, std::shared_ptr<const IUiFactory> uiFactory
		, std::shared_ptr<IProgressController> progressController
	)
		: m_settings(std::move(settings))
		, m_logicFactory(std::move(logicFactory))
		, m_uiFactory(std::move(uiFactory))
		, m_progressController(std::move(progressController))
	{
	}

	void CheckForUpdate(std::shared_ptr<IClient> client, Callback callback)
	{
		m_callback = std::move(callback);

		std::shared_ptr executor = m_logicFactory->GetExecutor();
		(*executor)({ "Check for app updates", [&, executor, client = std::move(client)] () mutable
		{
			Requester requester { CreateQtConnection("https://api.github.com") };
			requester.GetLatestRelease(client, "heimdallr", "books");
			const auto needUpdate = IsLatestReleaseNewer();

			return [&, executor = std::move(executor), needUpdate] (size_t) mutable
			{
				if (needUpdate)
					QTimer::singleShot(0, [&] { ShowUpdateMessage(); });
				else
					m_callback();

				executor.reset();
			};
		} });
	}

private: // IClient
	void HandleLatestRelease(const Release & release) override
	{
		m_release = release;
	}

	bool IsLatestReleaseNewer()
	{
		const auto nameSplitted = m_release.name.split(' ', Qt::SkipEmptyParts);
		if (nameSplitted.size() != 2)
			return false;

		if (m_settings->Get(DISCARDED_UPDATE_KEY, -1) == m_release.id)
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

private:
	void ShowUpdateMessage()
	{
		std::vector<std::pair<QMessageBox::ButtonRole, QString>> buttons
		{
			{ QMessageBox::AcceptRole, Tr(DOWNLOAD) },
			{ QMessageBox::DestructiveRole, Tr(VISIT_HOME) },
			{ QMessageBox::ActionRole, Tr(SKIP) },
			{ QMessageBox::RejectRole, Tr(CANCEL) },
		};
		if (m_release.assets.empty() || m_release.assets.front().browser_download_url.isEmpty())
			buttons.erase(buttons.begin());

		const auto defaultRole = buttons.front().first;

		switch (m_uiFactory->ShowCustomDialog(QMessageBox::Information, Loc::Tr(Loc::Ctx::COMMON, Loc::WARNING), Tr(RELEASED).arg(m_release.name), buttons, defaultRole))  // NOLINT(clang-diagnostic-switch-enum)
		{
			case QMessageBox::AcceptRole:
				return Download();
			case QMessageBox::DestructiveRole:
				QDesktopServices::openUrl(m_release.html_url);
				break;
			case QMessageBox::ActionRole:
				m_settings->Set(DISCARDED_UPDATE_KEY, m_release.id);
				break;
			case QMessageBox::RejectRole:
			case QMessageBox::NoRole:
				break;
			default:
				assert(false && "unexpected case");
		}

		m_callback();
	}

	void Download()
	{
		auto downloader = std::make_shared<Network::Downloader>();
		const auto installerFolder = m_uiFactory->GetExistingDirectory(Tr(INSTALLER_FOLDER));
		if (installerFolder.isEmpty())
			return m_callback();

		auto installerFileName = QString("%1/%2").arg(installerFolder, m_release.assets.front().name);
		auto file = std::make_shared<QFile>(installerFileName);
		file->open(QIODevice::WriteOnly);
		downloader->Download(m_release.assets.front().browser_download_url, *file, [&, downloader, file, installerFileName = std::move(installerFileName)] (int /*code*/, const QString & /*error*/) mutable
				{
					file->close();
					file.reset();
					const auto startInstaller = m_uiFactory->ShowQuestion(Tr(START_INSTALLER), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;

					QTimer::singleShot(0, [installerFileName = std::move(installerFileName)
						, downloader = std::move(downloader)
						, callback = std::move(m_callback), startInstaller
					] () mutable
					{
						downloader.reset();
						callback();
						if (startInstaller)
						{
							QProcess::startDetached(installerFileName);
							QCoreApplication::exit();
						}
					});
				}
			, [&, progressItem = std::shared_ptr<IProgressController::IProgressItem>{} , bytesReceivedLast = int64_t{0}] (const int64_t bytesReceived, const int64_t bytesTotal) mutable
				{
					if (!progressItem)
						progressItem = m_progressController->Add(bytesTotal);

					progressItem->Increment(bytesReceived - bytesReceivedLast);
					bytesReceivedLast = bytesReceived;
				});
	}

private:
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	std::shared_ptr<const ILogicFactory> m_logicFactory;
	std::shared_ptr<const IUiFactory> m_uiFactory;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;
	Callback m_callback;
	Release m_release;
};

UpdateChecker::UpdateChecker(std::shared_ptr<ISettings> settings
	, std::shared_ptr<ILogicFactory> logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
	, std::shared_ptr<IBooksExtractorProgressController> progressController
)
	: m_impl(std::make_shared<Impl>(std::move(settings)
		, std::move(logicFactory)
		, std::move(uiFactory)
		, std::move(progressController)
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
	m_impl->CheckForUpdate(m_impl, std::move(callback));

}
