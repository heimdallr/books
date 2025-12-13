#include "SingleInstanceController.h"

#include <QLocalServer>
#include <QLocalSocket>

#include "fnd/FindPair.h"
#include "fnd/observable.h"

#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"
#include "interface/logic/ISingleInstanceController.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT        = "SingleInstanceController";
constexpr auto DIALOG_MESSAGE = QT_TRANSLATE_NOOP("SingleInstanceController", "One instance of the program is already running. Do you want to start a new one?");

constexpr auto MULTIPLE_INSTANCE_APP_KEY = "ui/MultipleInstance";
constexpr auto SERVER_NAME               = "dd51de1b-7ef5-426b-baf0-180c20a6310e";
constexpr auto SERVER_MESSAGE            = "show";

#define MODE_ITEMS_X_MACRO \
	MODE_ITEM(Enabled)     \
	MODE_ITEM(Disabled)    \
	MODE_ITEM(Dialog)

enum class Mode
{
#define MODE_ITEM(NAME) NAME,
	MODE_ITEMS_X_MACRO
#undef MODE_ITEM
};

constexpr std::pair<const char*, Mode> MODES[] {
#define MODE_ITEM(NAME) { #NAME, Mode::NAME },
	MODE_ITEMS_X_MACRO
#undef MODE_ITEM
};

TR_DEF
} // namespace

struct SingleInstanceController::Impl final : Observable<IObserver>
{
	const Mode mode;
	bool       firstInstance { false };

	std::unique_ptr<QLocalServer> server;

	explicit Impl(const ISettings& settings, const IUiFactory& uiFactory)
		: mode { settings.Get(Constant::Settings::PREFER_HIDE_TO_TRAY_KEY, false) ? Mode::Disabled
		                                                                   : FindSecond(MODES, settings.Get(MULTIPLE_INSTANCE_APP_KEY, QString(MODES[0].first)).toStdString().data(), PszComparer {}) }
		, firstInstance { mode != Mode::Enabled && !TryToConnect(uiFactory) }
	{
		if (!firstInstance)
			return;

		server        = std::make_unique<QLocalServer>();
		firstInstance = server->listen(SERVER_NAME);
		QObject::connect(server.get(), &QLocalServer::newConnection, [this] {
			while (server->hasPendingConnections())
			{
				auto* socket = server->nextPendingConnection();
				QObject::connect(socket, &QLocalSocket::readyRead, [this, socket] {
					if (socket->readAll() == SERVER_MESSAGE)
						Perform(&IObserver::OnStartAnotherApp);
				});
			}
		});
	}

private:
	bool TryToConnect(const IUiFactory& uiFactory) const
	{
		QLocalSocket localSocket;
		localSocket.connectToServer(SERVER_NAME, QIODevice::WriteOnly);
		localSocket.waitForConnected(50);

		if (localSocket.state() != QLocalSocket::LocalSocketState::ConnectedState)
			return false;

		const auto exit = [&] {
			localSocket.write(SERVER_MESSAGE);
			localSocket.flush();
			localSocket.waitForBytesWritten();
			throw std::runtime_error("another app instance started");
		};

		if (mode == Mode::Disabled)
			exit();

		assert(mode == Mode::Dialog);

		switch (uiFactory.ShowQuestion(Tr(DIALOG_MESSAGE), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No)) // NOLINT(clang-diagnostic-switch-enum)
		{
			case QMessageBox::No:
				return exit(), true;

			case QMessageBox::Cancel:
				throw std::runtime_error("another app instance started");

			default:
				break;
		}

		return true;
	}
};

SingleInstanceController::SingleInstanceController(const std::shared_ptr<const ISettings>& settings, const std::shared_ptr<const IUiFactory>& uiFactory)
	: m_impl { *settings, *uiFactory }
{
}

SingleInstanceController::~SingleInstanceController() = default;

bool SingleInstanceController::IsFirstSingleInstanceApp() const
{
	return m_impl->firstInstance;
}

void SingleInstanceController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void SingleInstanceController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
