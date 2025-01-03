#include "OpdsController.h"

#include <QCoreApplication>
#include <QLocalSocket>
#include <QProcess>
#include <QTimer>

#include <plog/Log.h>

#include "fnd/observable.h"
#include "interface/constants/ProductConstant.h"

using namespace HomeCompa;
using namespace Flibrary;

struct OpdsController::Impl : Observable<IObserver>
{
	QLocalSocket socket;
	QTimer timer;

	Impl(const IOpdsController & /*controller*/)
	{
		QObject::connect(&socket, &QLocalSocket::connected, [&]
		{
			PLOGD << "OPDS connected";
			Perform(&IObserver::OnRunningChanged);
			timer.stop();
		});

		QObject::connect(&socket, &QLocalSocket::disconnected, [&]
		{
			PLOGD << "OPDS disconnected";
			Perform(&IObserver::OnRunningChanged);
		});

		QObject::connect(&socket, &QLocalSocket::errorOccurred, [&]
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
	PLOGD << "OpdsController created";
	m_impl->socket.connectToServer(Constant::OPDS_SERVER_NAME);
}

OpdsController::~OpdsController()
{
	PLOGD << "OpdsController destroyed";
}

bool OpdsController::IsRunning() const
{
	return m_impl->socket.state() == QLocalSocket::ConnectedState;
}

void OpdsController::Start()
{
	assert(!IsRunning());

	if (QProcess::startDetached(QCoreApplication::applicationDirPath() + "/opds.exe"))
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
	if (IsRunning())
		return;

	m_impl->socket.write(Constant::OPDS_SERVER_COMMAND_RESTART);
	m_impl->socket.flush();
	m_impl->socket.waitForBytesWritten();
	PLOGD << "OPDS " << Constant::OPDS_SERVER_COMMAND_RESTART << " sent";
}

void OpdsController::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void OpdsController::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}
