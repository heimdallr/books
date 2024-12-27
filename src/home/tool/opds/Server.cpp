#include "Server.h"

#include <QTcpServer>
#include <QHttpServer>
#include <QTimer>

#include <plog/Log.h>

#include "interface/constants/SettingsConstant.h"

#include "util/ISettings.h"

using namespace HomeCompa;
using namespace Opds;

struct Server::Impl
{
	QHttpServer server;
	Impl(const ISettings & settings
	)

	{
		auto tcpServer = std::make_unique<QTcpServer>();
		const auto port = settings.Get(Flibrary::Constant::Settings::OPDS_PORT_KEY, Flibrary::Constant::Settings::OPDS_PORT_DEFAULT).toUShort();
		if (!tcpServer->listen(QHostAddress::Any, port) || !server.bind(tcpServer.get()))
			throw std::runtime_error(std::format("Cannot listen port {}", port));

		PLOGD << "Listening port " << port;

		(void)tcpServer.release();

		server.addAfterRequestHandler(&server, [] (const QHttpServerRequest & request, QHttpServerResponse & resp)
		{
			PLOGD << request.value("Host") << " requests " << request.url().path() << request.query().toString();
			auto h = resp.headers();
			h.append(QHttpHeaders::WellKnownHeader::Server, "FLibrary HTTP Server");
			resp.setHeaders(std::move(h));
		});

		server.route("/opds", [] (const QHttpServerRequest & /*request*/)
		{
			constexpr auto t = R"(<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom" xmlns:dc="http://purl.org/dc/terms/" xmlns:opds="http://opds-spec.org/2010/catalog">
  <updated>2024-12-26T16:16:33Z</updated>
  <id>root</id>
  <title>Flibusta FB2 Local</title>
  <entry>
    <updated>2024-12-26T16:16:33Z</updated>
    <id>author</id>
    <title>author</title>
    <link href="/opds/author" rel="subsection" type="application/atom+xml;profile=opds-catalog;kind=navigation"/>
  </entry>
  <entry>
    <updated>2024-12-26T16:16:33Z</updated>
    <id>series</id>
    <title>series</title>
    <link href="/opds/series" rel="subsection" type="application/atom+xml;profile=opds-catalog;kind=navigation"/>
  </entry>
  <entry>
    <updated>2024-12-26T16:16:33Z</updated>
    <id>title</id>
    <title>title</title>
    <link href="/opds/title" rel="subsection" type="application/atom+xml;profile=opds-catalog;kind=navigation"/>
  </entry>
  <entry>
    <updated>2024-12-26T16:16:33Z</updated>
    <id>genre</id>
    <title>genre</title>
    <link href="/opds/genre" rel="subsection" type="application/atom+xml;profile=opds-catalog;kind=navigation"/>
  </entry>
</feed>
)";

			return QHttpServerResponse(QByteArray {t});
		});
		server.route("/opds/author/<arg>", [] (const QString & request)
		{
			return QHttpServerResponse(request.toUtf8());
		});
	}
};

Server::Server(const std::shared_ptr<const ISettings> & settings
)
	: m_impl(*settings
		)
{
	PLOGD << "Server created";
}

Server::~Server()
{
	PLOGD << "Server destroyed";
}
