#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include "interface/constants/Localization.h"

#include "network/network/downloader.h"
#include "network/rest/api/github/Release.h"
#include "network/rest/api/github/Requester.h"
#include "network/rest/connection/ConnectionFactory.h"
#include "util/IExecutor.h"
#include "util/app.h"

#include "log.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;
using namespace RestAPI;
using namespace Github;

namespace
{
constexpr auto DISCARDED_UPDATE_KEY  = "ui/Update/SkippedVersion";
constexpr auto LAST_UPDATE_CHECK_KEY = "ui/Update/LastCheck";
constexpr auto DIALOG_KEY            = "Installer";

constexpr auto CONTEXT             = "UpdateChecker";
constexpr auto INSTALL             = QT_TRANSLATE_NOOP("UpdateChecker", "Install %1");
constexpr auto DOWNLOAD            = QT_TRANSLATE_NOOP("UpdateChecker", "Download");
constexpr auto SKIP                = QT_TRANSLATE_NOOP("UpdateChecker", "Skip");
constexpr auto CANCEL              = QT_TRANSLATE_NOOP("UpdateChecker", "Cancel");
constexpr auto RELEASED            = QT_TRANSLATE_NOOP("UpdateChecker", "%1 released!<br/><a href='%2'><font size='%3px'>Visit release page</font></a>");
constexpr auto INSTALLER_FOLDER    = QT_TRANSLATE_NOOP("UpdateChecker", "Select folder for app installer");
constexpr auto START_INSTALLER     = QT_TRANSLATE_NOOP("UpdateChecker", "Run the installer?");
constexpr auto CHECK_FAILED        = QT_TRANSLATE_NOOP("UpdateChecker", "Update check failed");
constexpr auto VERSION_ACTUAL      = QT_TRANSLATE_NOOP("UpdateChecker", "Current version %1 is actual");
constexpr auto VERSION_MIRACLE     = QT_TRANSLATE_NOOP("UpdateChecker", "Last version %1, your version %2. Did a miracle happen?");
constexpr auto INSTALLER_NOT_FOUND = QT_TRANSLATE_NOOP("UpdateChecker", "Something strange, the installer file is missing. Visit download page?");

TR_DEF

enum class CheckResult
{
	Error,
	Discard,
	NeedUpdate,
	Actual,
	MoreActual,
};

} // namespace

class UpdateChecker::Impl final : virtual public IClient
{
public:
	Impl(
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<const Util::IUiFactory>     uiFactory,
		std::shared_ptr<ISettings>                  settings,
		std::shared_ptr<IProgressController>        progressController
	)
		: m_logicFactory { logicFactory }
		, m_uiFactory { std::move(uiFactory) }
		, m_settings { std::move(settings) }
		, m_progressController { std::move(progressController) }
	{
	}

	void CheckForUpdate(std::shared_ptr<IClient> client, const bool force, Callback callback)
	{
		if (!force && !NeedCheckUpdate())
			return callback();

		m_callback = std::move(callback);

		std::shared_ptr executor = ILogicFactory::Lock(m_logicFactory)->GetExecutor();
		(*executor)({ "Check for app updates", [this, executor, client = std::move(client), force]() mutable {
						 Requester requester { CreateQtConnection("https://api.github.com") };
						 requester.GetLatestRelease(client, "heimdallr", "books");
						 const auto checkResult = Check();

						 return [this, executor = std::move(executor), force, checkResult](size_t) mutable {
							 QTimer::singleShot(0, [&, force, checkResult] {
								 ShowMessage(force, checkResult);
							 });
							 executor.reset();
						 };
					 } });
	}

private: // IClient
	void HandleLatestRelease(const Release& release) override
	{
		m_release = release;
	}

private:
	bool NeedCheckUpdate()
	{
		const auto currentDateTime = QDateTime::currentDateTime();
		if (const auto lastCheckVar = m_settings->Get(LAST_UPDATE_CHECK_KEY); lastCheckVar.isValid())
			if (const auto lastCheckDateTime = QDateTime::fromString(lastCheckVar.toString(), Qt::ISODate); lastCheckDateTime.isValid() && lastCheckDateTime > currentDateTime.addDays(-1))
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
		if (!std::ranges::all_of(m_nameSplitted.back().split('.', Qt::SkipEmptyParts), [&](const QString& item) {
				bool ok = false;
				latestVersion.push_back(item.toInt(&ok));
				return ok;
			}))
			return CheckResult::Error;

		std::vector<int> currentVersion;
		std::ranges::transform(QString(PRODUCT_VERSION).split('.', Qt::SkipEmptyParts), std::back_inserter(currentVersion), [](const QString& item) {
			bool       ok    = false;
			const auto value = item.toInt(&ok);
			assert(ok);
			return value;
		});

		if (latestVersion.size() != currentVersion.size())
			return CheckResult::Error;

		return std::ranges::lexicographical_compare(currentVersion, latestVersion) ? CheckResult::NeedUpdate
		     : std::ranges::lexicographical_compare(latestVersion, currentVersion) ? CheckResult::MoreActual
		                                                                           : CheckResult::Actual;
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
		const auto installer = Util::GetInstallerDescription();

		std::vector<std::pair<QMessageBox::ButtonRole, QString>> buttons;
		if (!m_release.assets.empty() && !m_release.assets.front().browser_download_url.isEmpty())
		{
			if (installer.type != Util::InstallerType::portable)
				buttons.emplace_back(QMessageBox::AcceptRole, Tr(INSTALL).arg(m_release.name));
			buttons.emplace_back(QMessageBox::DestructiveRole, Tr(DOWNLOAD));
		}

		static constexpr std::pair<QMessageBox::ButtonRole, const char*> buttonSettings[] {
			{ QMessageBox::ActionRole,   SKIP },
			{ QMessageBox::RejectRole, CANCEL },
		};
		buttons.reserve(buttons.size() + std::size(buttonSettings));
		std::ranges::transform(buttonSettings, std::back_inserter(buttons), [](auto item) {
			return std::make_pair(item.first, Tr(item.second));
		});

		switch (m_uiFactory->ShowCustomDialog( // NOLINT(clang-diagnostic-switch-enum)
			QMessageBox::Information,
			Loc::Tr(Loc::Ctx::COMMON, Loc::WARNING),
			Tr(RELEASED).arg(m_release.name, m_release.html_url).arg(m_uiFactory->GetParentWidget()->font().pointSize() * 9 / 10),
			buttons,
			buttons.front().first
		))
		{
			case QMessageBox::AcceptRole:
				return Download(true);
			case QMessageBox::DestructiveRole:
				return Download();
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

	void Download(const bool silent = false)
	{
		const auto installer = Util::GetInstallerDescription();
		const auto it        = std::ranges::find_if(m_release.assets, [=](const Asset& asset) {
            const auto ext = QFileInfo(asset.name).suffix();
            return ext == installer.ext;
        });
		if (it == m_release.assets.end())
		{
			if (m_uiFactory->ShowWarning(Tr(INSTALLER_NOT_FOUND), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
				QDesktopServices::openUrl(m_release.html_url);
			return m_callback();
		}

		auto downloader     = std::make_shared<Network::Downloader>();
		auto downloadFolder = silent ? QStandardPaths::writableLocation(QStandardPaths::TempLocation) : m_uiFactory->GetExistingDirectory(DIALOG_KEY, Tr(INSTALLER_FOLDER));
		if (downloadFolder.isEmpty())
			return m_callback();

		auto downloadFileName = QString("%1/%2").arg(downloadFolder, it->name);
		auto file             = std::make_shared<QFile>(downloadFileName);
		file->open(QIODevice::WriteOnly);
		downloader->Download(
			it->browser_download_url,
			*file,
			[this, silent, downloader, installer, file, downloadFolder = std::move(downloadFolder), downloadFileName = std::move(downloadFileName)](size_t, const int code, const QString& error) mutable {
				file->close();
				file.reset();
				if (code != 0)
				{
					QFile::remove(downloadFileName);
					if (!error.isEmpty())
						(void)m_uiFactory->ShowWarning(error);
				}

				const auto startInstaller = installer.type != Util::InstallerType::portable && code == 0
			                             && (silent || m_uiFactory->ShowQuestion(Tr(START_INSTALLER), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes);

				QTimer::singleShot(
					0,
					[uiFactory        = m_uiFactory,
			         downloadFolder   = std::move(downloadFolder),
			         downloadFileName = std::move(downloadFileName),
			         downloader       = std::move(downloader),
			         callback         = std::move(m_callback),
			         installer,
			         startInstaller]() mutable {
						downloader.reset();
						callback();
						if (startInstaller
				            && (installer.type == Util::InstallerType::exe   ? QProcess::startDetached(downloadFileName)
				                : installer.type == Util::InstallerType::msi ? QDesktopServices::openUrl(QUrl::fromLocalFile(downloadFileName))
				                                                             : (assert(false), false)))
							return QCoreApplication::exit();

						QDesktopServices::openUrl(QUrl::fromLocalFile(downloadFolder));
					}
				);
			},
			[this, progressItem = std::shared_ptr<IProgressController::IProgressItem> {}, bytesReceivedLast = int64_t { 0 }](
				const int64_t bytesReceived,
				const int64_t bytesTotal,
				bool&         stopped
			) mutable //-V788
			{
				if (!progressItem)
					progressItem = m_progressController->Add(bytesTotal);

				progressItem->Increment(bytesReceived - bytesReceivedLast);
				bytesReceivedLast = bytesReceived;
				if (bytesReceived == bytesTotal)
					progressItem.reset();
				else
					stopped = progressItem->IsStopped();
			}
		);
	}

private:
	std::weak_ptr<const ILogicFactory>                      m_logicFactory;
	std::shared_ptr<const Util::IUiFactory>                 m_uiFactory;
	PropagateConstPtr<ISettings, std::shared_ptr>           m_settings;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;
	Callback                                                m_callback;
	Release                                                 m_release;
	QStringList                                             m_nameSplitted;
};

UpdateChecker::UpdateChecker(
	const std::shared_ptr<const ILogicFactory>&        logicFactory,
	std::shared_ptr<const Util::IUiFactory>            uiFactory,
	std::shared_ptr<ISettings>                         settings,
	std::shared_ptr<IBooksExtractorProgressController> progressController
)
	: m_impl(std::make_shared<Impl>(logicFactory, std::move(uiFactory), std::move(settings), std::move(progressController)))
{
	PLOGV << "UpdateChecker created";
}

UpdateChecker::~UpdateChecker()
{
	PLOGV << "UpdateChecker destroyed";
}

void UpdateChecker::CheckForUpdate(const bool force, Callback callback)
{
	m_impl->CheckForUpdate(m_impl, force, std::move(callback));
}
