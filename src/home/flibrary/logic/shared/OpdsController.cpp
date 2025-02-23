#include "OpdsController.h"

#include <QCoreApplication>
#include <QDir>
#include <QLocalSocket>
#include <QProcess>
#include <QSettings>
#include <QTimer>

#include "fnd/observable.h"

#include "interface/constants/ProductConstant.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto STARTUP_KEY = "CurrentVersion/Run/FLibrary OPDS server";

QString GetOpdsPath()
{
	return QCoreApplication::applicationDirPath() + "/opds.exe";
}

std::unique_ptr<QSettings> GetStartupSettings()
{
	return std::make_unique<QSettings>(QSettings::NativeFormat, QSettings::UserScope, "Microsoft", "Windows");
}

}

struct OpdsController::Impl : Observable<IObserver>
{
	QLocalSocket socket;
	QTimer timer;

	Impl(const IOpdsController& /*controller*/)
	{
		QObject::connect(&socket,
		                 &QLocalSocket::connected,
		                 [&]
		                 {
							 PLOGD << "OPDS connected";
							 Perform(&IObserver::OnRunningChanged);
							 timer.stop();
						 });

		QObject::connect(&socket,
		                 &QLocalSocket::disconnected,
		                 [&]
		                 {
							 PLOGD << "OPDS disconnected";
							 Perform(&IObserver::OnRunningChanged);
						 });

		QObject::connect(&socket,
		                 &QLocalSocket::errorOccurred,
		                 [&]
		                 {
							 PLOGD << "OPDS error occurred: " << socket.errorString();
							 Perform(&IObserver::OnRunningChanged);
						 });

		timer.setInterval(std::chrono::seconds(1));
		timer.setSingleShot(false);
		QObject::connect(&timer, &QTimer::timeout, [this] { socket.connectToServer(Constant::OPDS_SERVER_NAME); });
	}
};

OpdsController::OpdsController()
	: m_impl(*this)
{
	PLOGV << "OpdsController created";
	m_impl->socket.connectToServer(Constant::OPDS_SERVER_NAME);
}

OpdsController::~OpdsController()
{
	PLOGV << "OpdsController destroyed";
}

bool OpdsController::IsRunning() const
{
	return m_impl->socket.state() == QLocalSocket::ConnectedState;
}

void OpdsController::Start()
{
	assert(!IsRunning());

	if (QProcess::startDetached(GetOpdsPath()))
		m_impl->timer.start();
}

void OpdsController::Stop()
{
	assert(IsRunning());
	m_impl->socket.write(Constant::OPDS_SERVER_COMMAND_STOP);
	PLOGD << "OPDS " << Constant::OPDS_SERVER_COMMAND_STOP << " sent";
}

void OpdsController::Restart()
{
	if (!IsRunning())
		return;

	auto& socket = m_impl->socket;
	socket.write(Constant::OPDS_SERVER_COMMAND_RESTART);
	socket.flush();
	socket.waitForBytesWritten();
	PLOGD << "OPDS " << Constant::OPDS_SERVER_COMMAND_RESTART << " sent";
}

void OpdsController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void OpdsController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}

bool OpdsController::InStartup() const
{
	return GetStartupSettings()->contains(STARTUP_KEY);
}

void OpdsController::AddToStartup() const
{
	GetStartupSettings()->setValue(STARTUP_KEY, QDir::toNativeSeparators(GetOpdsPath()));
}

void OpdsController::RemoveFromStartup() const
{
	GetStartupSettings()->remove(STARTUP_KEY);
}
