#include "Server.h"

#include <QHttpServer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QtConcurrent>
#include <QTcpServer>

#include <plog/Log.h>

#include "interface/IRequester.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"

#include "util/ISettings.h"

using namespace HomeCompa;
using namespace Opds;

namespace {

constexpr auto ARG = "<arg>";
constexpr auto ROOT = "/opds";
constexpr auto BOOK_INFO = "/opds/Book/%1";
constexpr auto BOOK_DATA = "/opds/Book/data/%1";
constexpr auto BOOK_ZIP = "/opds/Book/zip/%1";
constexpr auto COVER = "/opds/Book/cover/%1";
constexpr auto THUMBNAIL = "/opds/Book/cover/thumbnail/%1";
constexpr auto NAVIGATION = "/opds/%1";
constexpr auto NAVIGATION_STARTS = "/opds/%1/starts/%2";
constexpr auto BOOK_LIST = "/opds/%1/Books/%2";
constexpr auto BOOK_LIST_STARTS = "/opds/%1/Books/%2/starts/%3";
constexpr auto AUTHOR_LIST = "/opds/%1/%2";
constexpr auto NAVIGATION_AUTHOR_STARTS = "/opds/%1/%2/starts/%3";
constexpr auto NAVIGATION_AUTHOR = "/opds/%1/%2/%3";
constexpr auto NAVIGATION_AUTHOR_BOOKS_STARTS = "/opds/%1/Authors/Books/%2/%3/starts/%4";

}

class Server::Impl
{
public:
	Impl(const ISettings & settings
		, std::shared_ptr<const IRequester> requester
	)
		: m_requester { std::move(requester) }
	{
		InitHttp(static_cast<uint16_t>(settings.Get(Flibrary::Constant::Settings::OPDS_PORT_KEY, Flibrary::Constant::Settings::OPDS_PORT_DEFAULT)));

		if (!m_localServer.listen(Flibrary::Constant::OPDS_SERVER_NAME))
			throw std::runtime_error(std::format("Cannot listen pipe {}", Flibrary::Constant::OPDS_SERVER_NAME));

		QObject::connect(&m_localServer, &QLocalServer::newConnection, [this]
		{
			auto * socket = m_localServer.nextPendingConnection();
			QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
			QObject::connect(socket, &QIODevice::readyRead, [socket]
			{
				const auto data = socket->readAll();
#ifndef NDEBUG
				PLOGD << data << " received";
#endif
				if (data.compare(Flibrary::Constant::OPDS_SERVER_COMMAND_STOP) == 0)
					return QCoreApplication::exit(0);

				if (data.compare(Flibrary::Constant::OPDS_SERVER_COMMAND_RESTART) == 0)
					return QCoreApplication::exit(Flibrary::Constant::RESTART_APP);
			});
		});
	}

private:
	void InitHttp(const uint16_t port)
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

		m_server.route(ROOT, [this]
		{
			return QtConcurrent::run([this]
			{
				QHttpServerResponse response(m_requester->GetRoot(ROOT));
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
				return response;
			});
		});

		m_server.route(QString(BOOK_INFO).arg(ARG), [this] (const QString & value)
		{
			return QtConcurrent::run([this, value]
			{
				QHttpServerResponse response(m_requester->GetBookInfo(QString(BOOK_INFO).arg(value), value));
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
				return response;
			});
		});

		m_server.route(QString(BOOK_DATA).arg(ARG), [this] (const QString & value)
		{
			return QtConcurrent::run([this, value]
			{
				QHttpServerResponse response(m_requester->GetBook(QString(BOOK_DATA).arg(value), value));
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/fb2");
				return response;
			});
		});

		m_server.route(QString(BOOK_ZIP).arg(ARG), [this] (const QString & value)
		{
			return QtConcurrent::run([this, value]
			{
				QHttpServerResponse response(m_requester->GetBookZip(QString(BOOK_ZIP).arg(value), value));
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/zip");
				return response;
			});
		});

		m_server.route(QString(COVER).arg(ARG), [this] (const QString & value)
		{
			return QtConcurrent::run([this, value]
			{
				QHttpServerResponse response(m_requester->GetCover(QString(COVER).arg(value), value));
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "image/jpeg");
				return response;
			});
		});

		m_server.route(QString(THUMBNAIL).arg(ARG), [this] (const QString & value)
		{
			return QtConcurrent::run([this, value]
			{
				QHttpServerResponse response(m_requester->GetCoverThumbnail(QString(THUMBNAIL).arg(value), value));
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "image/jpeg");
				return response;
			});
		});

		using RequesterNavigation = QByteArray(IRequester::*)(const QString &, const QString &) const;
		using RequesterBooks = QByteArray(IRequester::*)(const QString &, const QString &, const QString &) const;
		using RequesterNavigationAuthors = QByteArray(IRequester::*)(const QString &, const QString &, const QString &) const;
		using RequesterAuthorBooks = QByteArray(IRequester::*)(const QString &, const QString &, const QString &, const QString &) const;
		static constexpr std::pair<const char *, std::tuple<
				  RequesterNavigation
				, RequesterBooks
				, RequesterNavigationAuthors
				, RequesterAuthorBooks
			>> requesters[]
		{
#define		OPDS_ROOT_ITEM(NAME) {#NAME, {            \
				  &IRequester::Get##NAME##Navigation  \
				, &IRequester::Get##NAME##Books       \
				, &IRequester::Get##NAME##Authors     \
				, &IRequester::Get##NAME##AuthorBooks \
				}},
			OPDS_ROOT_ITEMS_X_MACRO
#undef		OPDS_ROOT_ITEM
		};

		for (const auto & [key, invokers] : requesters)
		{
			const auto & [navigationInvoker, booksInvoker, navigationAuthorsInvoker, authorBooksInvoker] = invokers;

			m_server.route(QString(NAVIGATION).arg(key), [this, key, invoker = navigationInvoker]
			{
				return QtConcurrent::run([this, key, invoker]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, QString(NAVIGATION).arg(key), QString {}));
					ReplaceOrAppendHeader (response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
					return response;
				});
			});

			m_server.route(QString(NAVIGATION_STARTS).arg(key, ARG), [this, key, invoker = navigationInvoker](const QString & value)
			{
				return QtConcurrent::run([this, key, invoker, value]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, QString(NAVIGATION_STARTS).arg(key, value), value.toUpper()));
					ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
					return response;
				});
			});

			m_server.route(QString(AUTHOR_LIST).arg(key, ARG), [this, key, invoker = navigationAuthorsInvoker] (const QString & navigationId)
			{
				return QtConcurrent::run([this, key, invoker, navigationId]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, QString(AUTHOR_LIST).arg(key, navigationId), navigationId.toUpper(), QString {}));
					ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
					return response;
				});
			});

			m_server.route(QString(BOOK_LIST).arg(key, ARG), [this, key, invoker = booksInvoker] (const QString & navigationId)
			{
				return QtConcurrent::run([this, key, invoker, navigationId]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, QString(AUTHOR_LIST).arg(key, navigationId), navigationId.toUpper(), QString {}));
					ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
					return response;
				});
			});

			m_server.route(QString(NAVIGATION_AUTHOR_STARTS).arg(key, ARG, ARG), [this, key, invoker = navigationAuthorsInvoker] (const QString & navigationId, const QString & value)
			{
				return QtConcurrent::run([this, key, invoker, navigationId, value]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, QString(NAVIGATION_AUTHOR_STARTS).arg(key, navigationId, value), navigationId.toUpper(), value.toUpper()));
					ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
					return response;
				});
			});

			m_server.route(QString(NAVIGATION_AUTHOR).arg(key, ARG, ARG), [this, key, invoker = authorBooksInvoker] (const QString & navigationId, const QString & authorId)
			{
				return QtConcurrent::run([this, key, invoker, navigationId, authorId]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, QString(NAVIGATION_AUTHOR_STARTS).arg(key, navigationId, authorId), navigationId.toUpper(), authorId.toUpper(), QString {}));
					ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
					return response;
				});
			});

			m_server.route(QString(NAVIGATION_AUTHOR_BOOKS_STARTS).arg(key, ARG, ARG, ARG), [this, key, invoker = authorBooksInvoker] (const QString & navigationId, const QString & authorId, const QString & value)
			{
				return QtConcurrent::run([this, key, invoker, navigationId, authorId, value]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, QString(NAVIGATION_AUTHOR_STARTS).arg(key, navigationId, authorId), navigationId.toUpper(), authorId.toUpper(), value));
					ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/atom+xml; charset=utf-8");
					return response;
				});
			});

			m_server.route(QString(BOOK_LIST_STARTS).arg(key, ARG, ARG), [this, key, invoker = booksInvoker] (const QString & navigationId, const QString & value)
			{
				return QtConcurrent::run([this, key, invoker, navigationId, value]
				{
					QHttpServerResponse response(std::invoke(invoker, *m_requester, QString(BOOK_LIST_STARTS).arg(key, navigationId, value), navigationId.toUpper(), value.toUpper()));
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
	QLocalServer m_localServer;
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
