#include "Server.h"

#include <QTcpServer>
#include <QHttpServer>
#include <QTimer>

#include <plog/Log.h>

#include "interface/IRequester.h"
#include "interface/constants/SettingsConstant.h"

#include "util/ISettings.h"

using namespace HomeCompa;
using namespace Opds;

class Server::Impl
{
public:
	Impl(const ISettings & settings
		, std::shared_ptr<const IRequester> requester
	)
		: m_requester { std::move(requester) }
	{
		Init(settings.Get(Flibrary::Constant::Settings::OPDS_PORT_KEY, Flibrary::Constant::Settings::OPDS_PORT_DEFAULT).toUShort());
	}

private:
	void Init(const uint16_t port)
	{
		auto tcpServer = std::make_unique<QTcpServer>();
		if (!tcpServer->listen(QHostAddress::Any, port) || !m_server.bind(tcpServer.get()))
			throw std::runtime_error(std::format("Cannot listen port {}", port));

		PLOGD << "Listening port " << port;

		(void)tcpServer.release();

		m_server.addAfterRequestHandler(&m_server, [] (const QHttpServerRequest & request, QHttpServerResponse & resp)
		{
			PLOGD << request.value("Host") << " requests " << request.url().path() << request.query().toString();
			auto h = resp.headers();
			h.append(QHttpHeaders::WellKnownHeader::Server, "FLibrary HTTP Server");
			resp.setHeaders(std::move(h));
		});

		m_server.route("/opds", [this] (const QHttpServerRequest & /*request*/)
		{
			return m_requester->GetRoot();
		});

		m_server.route("/opds/author/<arg>", [] (const QString & request)
		{
			return QHttpServerResponse(request.toUtf8());
		});
	}

private:
	QHttpServer m_server;
	std::shared_ptr<const IRequester> m_requester;
};

Server::Server(const std::shared_ptr<const ISettings> & settings
	, std::shared_ptr<IRequester> requester
)
	: m_impl(*settings
		, std::move(requester)
		)
{
	PLOGD << "Server created";
}

Server::~Server()
{
	PLOGD << "Server destroyed";
}
