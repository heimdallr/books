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
constexpr auto LAST_UPDATE_CHECK_KEY = "ui/Update/LastCheck";

constexpr auto CONTEXT           =                   "UpdateChecker";
constexpr auto DOWNLOAD          = QT_TRANSLATE_NOOP("UpdateChecker", "Download");
constexpr auto VISIT_HOME        = QT_TRANSLATE_NOOP("UpdateChecker", "Visit download page");
constexpr auto SKIP              = QT_TRANSLATE_NOOP("UpdateChecker", "Skip this version");
constexpr auto CANCEL            = QT_TRANSLATE_NOOP("UpdateChecker", "Cancel");
constexpr auto RELEASED          = QT_TRANSLATE_NOOP("UpdateChecker", "%1 released!");
constexpr auto INSTALLER_FOLDER  = QT_TRANSLATE_NOOP("UpdateChecker", "Select folder for app installer");
constexpr auto START_INSTALLER   = QT_TRANSLATE_NOOP("UpdateChecker", "Run the installer?");
constexpr auto CHECK_FAILED      = QT_TRANSLATE_NOOP("UpdateChecker", "Update check failed");
constexpr auto VERSION_ACTUAL    = QT_TRANSLATE_NOOP("UpdateChecker", "Current version %1 is actual");
constexpr auto VERSION_MIRACLE   = QT_TRANSLATE_NOOP("UpdateChecker", "Last version %1, your version %2. Did a miracle happen?");
TR_DEF

enum class CheckResult
{
	Error,
	Discard,
	NeedUpdate,
	Actual,
	MoreActual,
};

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

	void CheckForUpdate(std::shared_ptr<IClient> client, const bool force, Callback callback)
	{
		if (!force && !NeedCheckUpdate())
			return callback();

		m_callback = std::move(callback);

		std::shared_ptr executor = m_logicFactory->GetExecutor();
		(*executor)({ "Check for app updates", [&, executor, client = std::move(client), force] () mutable
		{
			Requester requester { CreateQtConnection("https://api.github.com") };
			requester.GetLatestRelease(client, "heimdallr", "books");
			const auto checkResult = Check();

			return [&, executor = std::move(executor), force, checkResult] (size_t) mutable
			{
				QTimer::singleShot(0, [&, force, checkResult] { ShowMessage(force, checkResult); });
				executor.reset();
			};
		} });
	}

private: // IClient
	void HandleLatestRelease(const Release & release) override
	{
		m_release = release;
	}

private:
	bool NeedCheckUpdate()
	{
		const auto currentDateTime = QDateTime::currentDateTime();
		if (const auto lastCheckVar = m_settings->Get(LAST_UPDATE_CHECK_KEY); lastCheckVar.isValid())
			if (const auto lastCheckDateTime = QDateTime::fromString(lastCheckVar.toString(), Qt::ISODate); lastCheckDateTime.isValid() && lastCheckDateTime.addDays(1) > currentDateTime)
				return false;

		m_settings->Set(LAST_UPDATE_CHECK_KEY, currentDateTime.toString(Qt::ISODate));
		return true;
	}

	CheckResult Check()
	{
		m_nameSplitted = m_release.name.split(' ', Qt::SkipEmptyParts);
		if (m_nameSplitted.size() != 2)
			return CheckResult::Error;

		if (m_settings->Get(DISCARDED_UPDATE_KEY, -1) == m_release.id)
			return CheckResult::Discard;

		std::vector<int> latestVersion;
		if (!std::ranges::all_of(m_nameSplitted.back().split('.', Qt::SkipEmptyParts), [&] (const QString & item)
		{
			bool ok = false;
			latestVersion.push_back(item.toInt(&ok));
			return ok;
		}))
			return CheckResult::Error;

		std::vector<int> currentVersion;
		std::ranges::transform(QString(PRODUCT_VERSION).split('.', Qt::SkipEmptyParts), std::back_inserter(currentVersion), [] (const QString & item)
		{
			bool ok = false;
			const auto value = item.toInt(&ok);
			assert(ok);
			return value;
		});

		if (latestVersion.size() != currentVersion.size())
			return CheckResult::Error;

		return
			  std::ranges::lexicographical_compare(currentVersion, latestVersion) ? CheckResult::NeedUpdate
			: std::ranges::lexicographical_compare(latestVersion, currentVersion) ? CheckResult::MoreActual
			:																		CheckResult::Actual
			;
	}

private:
	void ShowMessage(const bool force, const CheckResult checkResult)
	{
		QString message;
		switch (checkResult)
		{
			case CheckResult::NeedUpdate:
				return ShowUpdateMessage();

			case CheckResult::Error:
				message = Tr(CHECK_FAILED);
				break;

			case CheckResult::Discard:
				if (force)
					return ShowUpdateMessage();

				break;

			case CheckResult::Actual:
				message = Tr(VERSION_ACTUAL).arg(PRODUCT_VERSION);
				break;

			case CheckResult::MoreActual:
				message = Tr(VERSION_MIRACLE).arg(m_nameSplitted.back()).arg(PRODUCT_VERSION);
				break;
		}

		if (force)
			m_uiFactory->ShowInfo(message);

		m_callback();
	}

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
		downloader->Download(m_release.assets.front().browser_download_url, *file, [&, downloader, file, installerFileName = std::move(installerFileName)] (const int code, const QString & error) mutable
				{
					file->close();
					file.reset();
					if (code != 0)
					{
						QFile::remove(installerFileName);
						if (!error.isEmpty())
							(void)m_uiFactory->ShowWarning(error);
					}

					const auto startInstaller = code == 0 && m_uiFactory->ShowQuestion(Tr(START_INSTALLER), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;

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
			, [&, progressItem = std::shared_ptr<IProgressController::IProgressItem>{}, bytesReceivedLast = int64_t{0}] (const int64_t bytesReceived, const int64_t bytesTotal, bool & stopped) mutable
				{
					if (!progressItem)
						progressItem = m_progressController->Add(bytesTotal);

					progressItem->Increment(bytesReceived - bytesReceivedLast);
					bytesReceivedLast = bytesReceived;
					if (bytesReceived == bytesTotal)
						progressItem.reset();
					else
						stopped = progressItem->IsStopped();
				});
	}

private:
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	std::shared_ptr<const ILogicFactory> m_logicFactory;
	std::shared_ptr<const IUiFactory> m_uiFactory;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;
	Callback m_callback;
	Release m_release;
	QStringList m_nameSplitted;
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

void UpdateChecker::CheckForUpdate(const bool force, Callback callback)
{
	m_impl->CheckForUpdate(m_impl, force, std::move(callback));
}
