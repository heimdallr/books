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

	Impl(const IOpdsController & controller)
	{
		QObject::connect(&socket, &QLocalSocket::stateChanged, [&]
		{
			Perform(&IObserver::OnRunningChanged);
			if (controller.IsRunning())
				timer.stop();
		});

		timer.setInterval(std::chrono::seconds(1));
		timer.setSingleShot(false);
		QObject::connect(&timer, &QTimer::timeout, [this] { socket.connectToServer(Constant::OPDS_SERVER_NAME); });
		timer.start();
	}
};

OpdsController::OpdsController()
	: m_impl(*this)
{
	PLOGD << "OpdsController created";
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
}

void OpdsController::Restart()
{
	if (IsRunning())
		m_impl->socket.write(Constant::OPDS_SERVER_COMMAND_RESTART);
}

void OpdsController::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void OpdsController::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}
