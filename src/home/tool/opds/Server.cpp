#include "Server.h"

#include <QHttpServer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTcpServer>
#include <QtConcurrent>

#include "interface/IRequester.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"

#include "util/ISettings.h"

#include "log.h"
#include "root.h"

using namespace HomeCompa;
using namespace Opds;

namespace
{

constexpr auto ARG = "<arg>";
constexpr auto ROOT = "%1";
constexpr auto BOOK_INFO = "%1/Book/%2";
constexpr auto BOOK_DATA = "%1/Book/data/%2";
constexpr auto BOOK_ZIP = "%1/Book/zip/%2";
constexpr auto COVER = "%1/Book/cover/%2";
constexpr auto THUMBNAIL = "%1/Book/cover/thumbnail/%2";
constexpr auto NAVIGATION = "%1/%2";
constexpr auto NAVIGATION_STARTS = "%1/%2/starts/%3";
constexpr auto BOOK_LIST = "%1/%2/Books/%3";
constexpr auto BOOK_LIST_STARTS = "%1/%2/Books/%3/starts/%4";
constexpr auto AUTHOR_LIST = "%1/%2/%3";
constexpr auto NAVIGATION_AUTHOR_STARTS = "%1/%2/%3/starts/%4";
constexpr auto NAVIGATION_AUTHOR = "%1/%2/%3/%4";
constexpr auto NAVIGATION_AUTHOR_BOOKS_STARTS = "%1/%2/Authors/Books/%3/%4/starts/%5";
constexpr auto READ = "%1/read/%2";

void ReplaceOrAppendHeader(QHttpServerResponse& response, const QHttpHeaders::WellKnownHeader key, const QString& value)
{
	auto h = response.headers();
	h.replaceOrAppend(key, value);
	response.setHeaders(std::move(h));
}

enum class MessageType
{
	Atom,
	Read,
};

constexpr const char* ROOTS[] { "/opds", "/web" };

constexpr const char* CONTENT_TYPES[][std::size(ROOTS)] {
	{ "application/atom+xml; charset=utf-8", "text/html; charset=utf-8" },
	{							   nullptr, "text/html; charset=utf-8" },
};

void SetContentType(QHttpServerResponse& response, const QString& root, const MessageType type)
{
	const auto rootIndex = std::distance(std::begin(ROOTS), std::ranges::find_if(ROOTS, [root = root.toStdString()](const char* item) { return root == item; }));
	const auto* contentType = CONTENT_TYPES[static_cast<size_t>(type)][rootIndex];
	assert(contentType);
	ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, contentType);
}

} // namespace

class Server::Impl
{
public:
	Impl(const ISettings& settings, std::shared_ptr<const IRequester> requester)
		: m_requester { std::move(requester) }
	{
		InitHttp(static_cast<uint16_t>(settings.Get(Flibrary::Constant::Settings::OPDS_PORT_KEY, Flibrary::Constant::Settings::OPDS_PORT_DEFAULT)));

		if (!m_localServer.listen(Flibrary::Constant::OPDS_SERVER_NAME))
			throw std::runtime_error(std::format("Cannot listen pipe {}", Flibrary::Constant::OPDS_SERVER_NAME));

		QObject::connect(&m_localServer,
		                 &QLocalServer::newConnection,
		                 [this]
		                 {
							 auto* socket = m_localServer.nextPendingConnection();
							 QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
							 QObject::connect(socket,
			                                  &QIODevice::readyRead,
			                                  [socket]
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

		m_server.addAfterRequestHandler(&m_server,
		                                [this](const QHttpServerRequest& request, QHttpServerResponse& resp)
		                                {
											PLOGD << request.value("Host") << " requests " << request.url().path() << request.query().toString();
											ReplaceOrAppendHeader(resp, QHttpHeaders::WellKnownHeader::Server, "FLibrary HTTP Server");
										});

		for (const auto& root : {
#define OPDS_REQUEST_ROOT_ITEM(NAME) "/" #NAME,
				 OPDS_REQUEST_ROOT_ITEMS_X_MACRO
#undef OPDS_REQUEST_ROOT_ITEM
			 })
			InitHttp(root);
	}

	void InitHttp(const QString& root)
	{
		m_server.route(QString(ROOT).arg(root),
		               [this, root]
		               {
						   return QtConcurrent::run(
							   [this, root]
							   {
								   QHttpServerResponse response(m_requester->GetRoot(root, QString(ROOT).arg(root)));
								   SetContentType(response, root, MessageType::Atom);
								   return response;
							   });
					   });

		m_server.route(QString(BOOK_INFO).arg(root, ARG),
		               [this, root](const QString& value)
		               {
						   return QtConcurrent::run(
							   [this, root, value]
							   {
								   QHttpServerResponse response(m_requester->GetBookInfo(root, QString(BOOK_INFO).arg(root, value), value));
								   SetContentType(response, root, MessageType::Atom);
								   return response;
							   });
					   });

		m_server.route(QString(BOOK_DATA).arg(root, ARG),
		               [this, root](const QString& value)
		               {
						   return QtConcurrent::run(
							   [this, root, value]
							   {
								   QHttpServerResponse response(m_requester->GetBook(root, QString(BOOK_DATA).arg(root, value), value));
								   ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/fb2");
								   return response;
							   });
					   });

		m_server.route(QString(READ).arg(root, ARG),
		               [this, root](const QString& value)
		               {
						   return QtConcurrent::run(
							   [this, root, value]
							   {
								   QHttpServerResponse response(m_requester->GetBookText(root, value));
								   SetContentType(response, root, MessageType::Read);
								   return response;
							   });
					   });

		m_server.route(QString(BOOK_ZIP).arg(root, ARG),
		               [this, root](const QString& value)
		               {
						   return QtConcurrent::run(
							   [this, root, value]
							   {
								   QHttpServerResponse response(m_requester->GetBookZip(root, QString(BOOK_ZIP).arg(root, value), value));
								   ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/zip");
								   return response;
							   });
					   });

		m_server.route(QString(COVER).arg(root, ARG),
		               [this, root](const QString& value)
		               {
						   return QtConcurrent::run(
							   [this, root, value]
							   {
								   QHttpServerResponse response(m_requester->GetCover(root, QString(COVER).arg(root, value), value));
								   ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "image/jpeg");
								   return response;
							   });
					   });

		m_server.route(QString(THUMBNAIL).arg(root, ARG),
		               [this, root](const QString& value)
		               {
						   return QtConcurrent::run(
							   [this, root, value]
							   {
								   QHttpServerResponse response(m_requester->GetCoverThumbnail(root, QString(THUMBNAIL).arg(root, value), value));
								   ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "image/jpeg");
								   return response;
							   });
					   });

		using RequesterNavigation = QByteArray (IRequester::*)(const QString&, const QString&, const QString&) const;
		using RequesterNavigationAuthors = QByteArray (IRequester::*)(const QString&, const QString&, const QString&, const QString&) const;
		using RequesterAuthorBooks = QByteArray (IRequester::*)(const QString&, const QString&, const QString&, const QString&, const QString&) const;
		// clang-format off
		static constexpr std::pair<const char*, std::tuple<RequesterNavigation, RequesterNavigationAuthors, RequesterAuthorBooks>> requesters[]
		{
#define		OPDS_ROOT_ITEM(NAME) {#NAME, {            \
				  &IRequester::Get##NAME##Navigation  \
				, &IRequester::Get##NAME##Authors     \
				, &IRequester::Get##NAME##AuthorBooks \
				}},
			OPDS_ROOT_ITEMS_X_MACRO
#undef		OPDS_ROOT_ITEM
		};
		// clang-format on

		for (const auto& [key, invokers] : requesters)
		{
			const auto& [navigationInvoker, navigationAuthorsInvoker, authorBooksInvoker] = invokers;

			m_server.route(QString(NAVIGATION).arg(root, key),
			               [this, root, key, invoker = navigationInvoker]
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker]
								   {
									   QHttpServerResponse response(std::invoke(invoker, *m_requester, std::cref(root), QString(NAVIGATION).arg(root, key), QString {}));
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(QString(NAVIGATION_STARTS).arg(root, key, ARG),
			               [this, root, key, invoker = navigationInvoker](const QString& value)
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker, value]
								   {
									   QHttpServerResponse response(std::invoke(invoker, *m_requester, std::cref(root), QString(NAVIGATION_STARTS).arg(root, key, value), value.toUpper()));
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(QString(AUTHOR_LIST).arg(root, key, ARG),
			               [this, root, key, invoker = navigationAuthorsInvoker](const QString& navigationId)
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker, navigationId]
								   {
									   QHttpServerResponse response(std::invoke(invoker, *m_requester, std::cref(root), QString(AUTHOR_LIST).arg(root, key, navigationId), navigationId.toUpper(), QString {}));
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(QString(BOOK_LIST).arg(root, key, ARG),
			               [this, root, key, invoker = authorBooksInvoker](const QString& navigationId)
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker, navigationId]
								   {
									   QHttpServerResponse response(
										   std::invoke(invoker, *m_requester, std::cref(root), QString(AUTHOR_LIST).arg(root, key, navigationId), navigationId.toUpper(), QString {}, QString {}));
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(QString(NAVIGATION_AUTHOR_STARTS).arg(root, key, ARG, ARG),
			               [this, root, key, invoker = navigationAuthorsInvoker](const QString& navigationId, const QString& value)
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker, navigationId, value]
								   {
									   QHttpServerResponse response(
										   std::invoke(invoker, *m_requester, std::cref(root), QString(NAVIGATION_AUTHOR_STARTS).arg(root, key, navigationId, value), navigationId.toUpper(), value.toUpper()));
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(QString(NAVIGATION_AUTHOR).arg(root, key, ARG, ARG),
			               [this, root, key, invoker = authorBooksInvoker](const QString& navigationId, const QString& authorId)
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker, navigationId, authorId]
								   {
									   QHttpServerResponse response(std::invoke(invoker,
					                                                            *m_requester,
					                                                            std::cref(root),
					                                                            QString(NAVIGATION_AUTHOR_STARTS).arg(root, key, navigationId, authorId),
					                                                            navigationId.toUpper(),
					                                                            authorId.toUpper(),
					                                                            QString {}));
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(QString(NAVIGATION_AUTHOR_BOOKS_STARTS).arg(root, key, ARG, ARG, ARG),
			               [this, root, key, invoker = authorBooksInvoker](const QString& navigationId, const QString& authorId, const QString& value)
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker, navigationId, authorId, value]
								   {
									   QHttpServerResponse response(std::invoke(invoker,
					                                                            *m_requester,
					                                                            std::cref(root),
					                                                            QString(NAVIGATION_AUTHOR_STARTS).arg(root, key, navigationId, authorId),
					                                                            navigationId.toUpper(),
					                                                            authorId.toUpper(),
					                                                            value));
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(
				QString(BOOK_LIST_STARTS).arg(root, key, ARG, ARG),
				[this, root, key, invoker = authorBooksInvoker](const QString& navigationId, const QString& value)
				{
					return QtConcurrent::run(
						[this, root, key, invoker, navigationId, value]
						{
							QHttpServerResponse response(
								std::invoke(invoker, *m_requester, std::cref(root), QString(BOOK_LIST_STARTS).arg(root, key, navigationId, value), navigationId.toUpper(), QString {}, value.toUpper()));
							SetContentType(response, root, MessageType::Atom);
							return response;
						});
				});
		}
	}

private:
	QLocalServer m_localServer;
	QHttpServer m_server;
	std::shared_ptr<const IRequester> m_requester;
};

Server::Server(const std::shared_ptr<const ISettings>& settings, std::shared_ptr<IRequester> requester)
	: m_impl(*settings, std::move(requester))
{
	PLOGV << "Server created";
}

Server::~Server()
{
	PLOGV << "Server destroyed";
}
