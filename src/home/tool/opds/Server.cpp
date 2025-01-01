#include "Server.h"

#include <QTcpServer>
#include <QHttpServer>
#include <QtConcurrent>

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

		m_server.addAfterRequestHandler(&m_server, [this] (const QHttpServerRequest & request, QHttpServerResponse & resp)
		{
			PLOGD << request.value("Host") << " requests " << request.url().path() << request.query().toString();
			ReplaceOrAppendHeader(resp, QHttpHeaders::WellKnownHeader::Server, "FLibrary HTTP Server");
		});

		m_server.route("/opds", [this]
		{
			return QtConcurrent::run([this]
			{
				QHttpServerResponse response(m_requester->GetRoot());
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
				return response;
			});
		});

		m_server.route("/opds/Book/<arg>", [this] (const QString & value)
		{
			return QtConcurrent::run([this, value]
			{
				QHttpServerResponse response(m_requester->GetBookInfo(value));
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
				return response;
			});
		});

		m_server.route("/opds/Book/data/<arg>", [this] (const QString & value)
		{
			return QtConcurrent::run([this, value]
			{
				QHttpServerResponse response(m_requester->GetBook(value));
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/fb2");
				return response;
			});
		});

		m_server.route("/opds/Book/zip/<arg>", [this] (const QString & value)
		{
			return QtConcurrent::run([this, value]
			{
				QHttpServerResponse response(m_requester->GetBookZip(value));
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/zip");
				return response;
			});
		});

		m_server.route("/opds/Book/cover/thumbnail/<arg>", [this] (const QString & value)
		{
			return QtConcurrent::run([this, value]
			{
				QHttpServerResponse response(m_requester->GetCoverThumbnail(value));
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "image/jpeg");
				return response;
			});
		});

		using RequesterNavigation = QByteArray(IRequester::*)(const QString &) const;
		using RequesterBooks = QByteArray(IRequester::*)(const QString &, const QString &) const;
		static constexpr std::pair<const char *, std::tuple<
				  RequesterNavigation
				, RequesterBooks
			>> requesters[]
		{
#define		OPDS_ROOT_ITEM(NAME) {#NAME, { &IRequester::Get##NAME##Navigation, &IRequester::Get##NAME##Books  }},
			OPDS_ROOT_ITEMS_X_MACRO
#undef		OPDS_ROOT_ITEM
		};

		for (const auto & [key, invokers] : requesters)
		{
			const auto & [navigationInvoker, booksInvoker] = invokers;

			m_server.route(QString("/opds/%1").arg(key), [this, invoker = navigationInvoker]
			{
				return QtConcurrent::run([this, invoker]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, QString {}));
					ReplaceOrAppendHeader (response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
					return response;
				});
			});

			m_server.route(QString("/opds/%1/starts/").arg(key), [this, invoker = navigationInvoker]
			{
				return QtConcurrent::run([this, invoker]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, QString {}));
					ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
					return response;
				});
			});

			m_server.route(QString("/opds/%1/starts/<arg>").arg(key), [this, invoker = navigationInvoker](const QString & value)
			{
				return QtConcurrent::run([this, invoker, value]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, value.toUpper()));
					ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
					return response;
				});
			});

			m_server.route(QString("/opds/%1/<arg>").arg(key), [this, invoker = booksInvoker] (const QString & navigationId)
			{
				return QtConcurrent::run([this, invoker, navigationId]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, navigationId.toUpper(), QString {}));
					ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
					return response;
				});
			});

			m_server.route(QString("/opds/%1/Books/<arg>/starts/<arg>").arg(key), [this, invoker = booksInvoker] (const QString & navigationId, const QString & value)
			{
				return QtConcurrent::run([this, invoker, navigationId, value]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, navigationId.toUpper(), value.toUpper()));
					ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
					return response;
				});
			});
		}
	}

	static void ReplaceOrAppendHeader(QHttpServerResponse & response, const QHttpHeaders::WellKnownHeader key, const QString & value)
	{
		auto h = response.headers();
		h.replaceOrAppend(key, value);
		response.setHeaders(std::move(h));
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
