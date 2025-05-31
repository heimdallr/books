#include "Server.h"

#include <QHttpServer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTcpServer>
#include <QtConcurrent>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "interface/IReactAppRequester.h"
#include "interface/IRequester.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"

#include "util/ISettings.h"

#include "log.h"
#include "root.h"
#include "zip.h"

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
constexpr auto SEARCH = "%1/search";
constexpr auto FAVICON = "/favicon.ico";
constexpr auto ASSETS = "/assets/%1";

constexpr auto GET_BOOKS_API = "/main/getBooks/%1";
constexpr auto GET_BOOKS_API_COVER = "/Images/covers/%1";
constexpr auto GET_BOOKS_API_BOOK_DATA = "/Images/fb2/%1";
constexpr auto GET_BOOKS_API_BOOK_ZIP = "/Images/zip/%1";
constexpr auto GET_BOOKS_API_BOOK_DATA_COMPACT = "/Images/fb2compact/%1";

#define OPDS_REQUEST_ROOT_ITEM(NAME) constexpr auto (NAME) = "/" #NAME;
OPDS_REQUEST_ROOT_ITEMS_X_MACRO
#undef OPDS_REQUEST_ROOT_ITEM

void ReplaceOrAppendHeader(QHttpServerResponse& response, const QHttpHeaders::WellKnownHeader key, const QString& value)
{
	auto h = response.headers();
	[[maybe_unused]] const auto ok = h.replaceOrAppend(key, value);
	assert(ok);
	response.setHeaders(std::move(h));
}

void ReplaceOrAppendHeader(QHttpServerResponse& response, const QString& key, const QString& value)
{
	auto h = response.headers();
	[[maybe_unused]] const auto ok = h.replaceOrAppend(key, value);
	assert(ok);
	response.setHeaders(std::move(h));
}

enum class MessageType
{
	Atom,
	Read,
};

constexpr const char* ROOTS[] { opds, web };

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

QHttpServerResponse EncodeContent(QByteArray src, const QString& acceptEncoding)
{
	if (!acceptEncoding.contains("gzip"))
		return QHttpServerResponse { src };

	QByteArray zipped;
	{
		QBuffer stream(&zipped);
		const ScopedCall streamGuard([&] { stream.open(QIODevice::WriteOnly); }, [&] { stream.close(); });
		Zip zip(stream, Zip::Format::GZip);
		zip.SetProperty(Zip::PropertyId::CompressionMethod, QVariant::fromValue(Zip::CompressionMethod::Deflate));
		zip.SetProperty(Zip::PropertyId::CompressionLevel, QVariant::fromValue(Zip::CompressionLevel::Fast));
		zip.Write({
			{ "file", std::move(src) }
        });
	}

	QHttpServerResponse result { zipped };
	ReplaceOrAppendHeader(result, QHttpHeaders::WellKnownHeader::ContentEncoding, "gzip");
	return result;
}

std::optional<QHttpServerResponse>
FromFile(const QString& fileName, const QString& acceptEncoding, const QString& contentType, const std::function<QByteArray(QByteArray)>& dataUpdater = [](QByteArray data) { return data; })
{
	QFile file(fileName);
	if (!file.exists())
		return std::nullopt;

	const auto ok = file.open(QIODevice::ReadOnly);
	if (!ok)
		return std::nullopt;

	QHttpServerResponse response(EncodeContent(dataUpdater(file.readAll()), acceptEncoding));
	ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, contentType);
	return response;
}

std::optional<QHttpServerResponse> FromWebsite(const QString& fileName, const QString& acceptEncoding, const std::function<QByteArray(QByteArray)>& dataUpdater = [](QByteArray data) { return data; })
{
	static constexpr std::pair<const char*, const char*> types[] {
		{ "html", "text/html; charset=utf-8" },
		{   "js",          "text/javascript" },
		{  "css",				 "text/css" },
	};
	const auto& contentType = FindSecond(types, QFileInfo(fileName).suffix().toStdString().data(), PszComparer {});
	if (auto result = FromFile(QString("%1/website/%2").arg(QCoreApplication::applicationDirPath(), fileName), acceptEncoding, contentType, dataUpdater))
		return result;

	return FromFile(QString(":/website/%1").arg(fileName), acceptEncoding, contentType, dataUpdater);
}

QString GetAcceptEncoding(const QHttpServerRequest& request)
{
	return QString::fromUtf8(request.headers().value(QHttpHeaders::WellKnownHeader::AcceptEncoding));
}

} // namespace

class Server::Impl
{
public:
	Impl(const ISettings& settings,
	     std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
	     std::shared_ptr<const IRequester> requester,
	     std::shared_ptr<const IReactAppRequester> reactAppRequester,
	     std::shared_ptr<const INoSqlRequester> noSqlRequester)
		: m_collectionProvider { std::move(collectionProvider) }
		, m_requester { std::move(requester) }
		, m_reactAppRequester { std::move(reactAppRequester) }
		, m_noSqlRequester { std::move(noSqlRequester) }
	{
		const auto host = [&]() -> QHostAddress
		{
			const auto address = settings.Get(Flibrary::Constant::Settings::OPDS_HOST_KEY, Flibrary::Constant::Settings::OPDS_HOST_DEFAULT);
			return address == Flibrary::Constant::Settings::OPDS_HOST_DEFAULT ? QHostAddress::Any : QHostAddress { address };
		}();
		InitHttp(host, static_cast<uint16_t>(settings.Get(Flibrary::Constant::Settings::OPDS_PORT_KEY, Flibrary::Constant::Settings::OPDS_PORT_DEFAULT)));

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
	using BookGetter = std::pair<QString, QByteArray> (INoSqlRequester::*)(const QString& bookId, bool restoreImages) const;

	auto GetBook(BookGetter getter, QString value, QString acceptEncoding, QString contentType, const bool restoreImages) const
	{
		return QtConcurrent::run(
			[this, getter, value = std::move(value), acceptEncoding = std::move(acceptEncoding), contentType = std::move(contentType), restoreImages]
			{
				auto [fileName, body] = std::invoke(getter, *m_noSqlRequester, std::cref(value), restoreImages);
				auto response = EncodeContent(std::move(body), acceptEncoding);
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, contentType);
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentDisposition, QString(R"(attachment; filename="%1")").arg(QUrl::toPercentEncoding(fileName)));
				ReplaceOrAppendHeader(response, "content-description", "File Transfer");
				ReplaceOrAppendHeader(response, "content-transfer-encoding", "binary");
				return response;
			});
	}

	void InitHttp(const QHostAddress& host, const uint16_t port)
	{
		auto tcpServer = std::make_unique<QTcpServer>();
		if (!tcpServer->listen(host, port) || !m_server.bind(tcpServer.get()))
			throw std::runtime_error(std::format("Cannot listen port {}", port));

		PLOGD << "Listening " << tcpServer->serverAddress().toString() << ":" << port;

		(void)tcpServer.release();

		m_server.addAfterRequestHandler(
			&m_server,
			[this](const QHttpServerRequest& request, QHttpServerResponse& resp)
			{
				const auto log = QString("%1 requests %2%3").arg(request.remoteAddress().toString(), request.url().path(), request.query().isEmpty() ? "" : QString("?%1").arg(request.query().toString()));
				PLOGD << log;

				ReplaceOrAppendHeader(resp, QHttpHeaders::WellKnownHeader::Server, "FLibrary HTTP Server");
				ReplaceOrAppendHeader(resp, QHttpHeaders::WellKnownHeader::Date, QDateTime::currentDateTime().toUTC().toString("ddd, dd MMM yyyy hh:mm:ss") + " GMT");
				ReplaceOrAppendHeader(resp, QHttpHeaders::WellKnownHeader::Connection, "keep-alive");
				ReplaceOrAppendHeader(resp, QHttpHeaders::WellKnownHeader::KeepAlive, "timeout=5");
			});

		Route();
	}

	void Route()
	{
		m_server.route(FAVICON,
		               [this]
		               {
						   return QtConcurrent::run(
							   [this]
							   {
								   auto response = *FromFile(":/icons/main.ico", "", "image/x-icon");
								   ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "image/x-icon");
								   ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::CacheControl, "public, max-age=0");
								   return response;
							   });
					   });

		for (const auto& root : {
#define OPDS_REQUEST_ROOT_ITEM(NAME) "/" #NAME,
				 OPDS_REQUEST_ROOT_ITEMS_X_MACRO
#undef OPDS_REQUEST_ROOT_ITEM
			 })
			RouteWithRoot(root);

		RouteReactApp();
	}

	void RouteWithRoot(const QString& root)
	{
		m_server.route(QString(ROOT).arg(root),
		               [this, root](const QHttpServerRequest& request)
		               {
						   return QtConcurrent::run(
							   [this, root, acceptEncoding = GetAcceptEncoding(request)]
							   {
								   auto response = EncodeContent(m_requester->GetRoot(root, QString(ROOT).arg(root)), acceptEncoding);
								   SetContentType(response, root, MessageType::Atom);
								   return response;
							   });
					   });

		m_server.route(QString(SEARCH).arg(root),
		               [this, root](const QHttpServerRequest& request)
		               {
						   const auto query = request.query();
						   QString q = query.queryItemValue("q", QUrl::FullyDecoded), start = query.queryItemValue("start", QUrl::FullyDecoded);
						   if (q.isEmpty())
							   if (const auto& parameters = query.queryItems(); !parameters.isEmpty())
								   q = parameters.front().second;

						   if (q.isEmpty())
						   {
							   assert(false);
							   PLOGE << "search term is empty";
						   }

						   return QtConcurrent::run(
							   [this, root, q, start, acceptEncoding = GetAcceptEncoding(request)]
							   {
								   auto response = EncodeContent(m_requester->Search(root, QString("%1?q=%2").arg(QString(SEARCH).arg(root)).arg(q), q, start), acceptEncoding);
								   SetContentType(response, root, MessageType::Atom);
								   return response;
							   });
					   });

		m_server.route(QString(BOOK_INFO).arg(root, ARG),
		               [this, root](const QString& value, const QHttpServerRequest& request)
		               {
						   return QtConcurrent::run(
							   [this, root, value, acceptEncoding = GetAcceptEncoding(request)]
							   {
								   auto response = EncodeContent(m_requester->GetBookInfo(root, QString(BOOK_INFO).arg(root, value), value), acceptEncoding);
								   SetContentType(response, root, MessageType::Atom);
								   return response;
							   });
					   });

		m_server.route(QString(BOOK_DATA).arg(root, ARG),
		               [this](const QString& value, const QHttpServerRequest& request) { return GetBook(&INoSqlRequester::GetBook, value, GetAcceptEncoding(request), "application/fb2", true); });

		m_server.route(QString(BOOK_ZIP).arg(root, ARG), [this](const QString& value) { return GetBook(&INoSqlRequester::GetBookZip, value, "", "application/zip", true); });

		m_server.route(QString(READ).arg(root, ARG),
		               [this, root](const QString& value, const QHttpServerRequest& request)
		               {
						   return QtConcurrent::run(
							   [this, root, value, acceptEncoding = GetAcceptEncoding(request)]
							   {
								   auto response = EncodeContent(m_requester->GetBookText(root, value), acceptEncoding);
								   SetContentType(response, root, MessageType::Read);
								   return response;
							   });
					   });

		m_server.route(QString(COVER).arg(root, ARG),
		               [this](const QString& value)
		               {
						   return QtConcurrent::run(
							   [this, value]
							   {
								   QHttpServerResponse response(m_noSqlRequester->GetCover(value));
								   ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "image/jpeg");
								   return response;
							   });
					   });

		m_server.route(QString(THUMBNAIL).arg(root, ARG),
		               [this](const QString& value)
		               {
						   return QtConcurrent::run(
							   [this, value]
							   {
								   QHttpServerResponse response(m_noSqlRequester->GetCoverThumbnail(value));
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

			{
				auto self = QString(NAVIGATION).arg(root, key);
				m_server.route(self,
				               [this, root, self, invoker = navigationInvoker](const QHttpServerRequest& request)
				               {
								   return QtConcurrent::run(
									   [this, root, self, invoker, acceptEncoding = GetAcceptEncoding(request)]
									   {
										   auto response = EncodeContent(std::invoke(invoker, *m_requester, std::cref(root), std::cref(self), QString {}), acceptEncoding);
										   SetContentType(response, root, MessageType::Atom);
										   return response;
									   });
							   });
			}

			m_server.route(QString(NAVIGATION_STARTS).arg(root, key, ARG),
			               [this, root, key, invoker = navigationInvoker](const QString& value, const QHttpServerRequest& request)
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker, value, acceptEncoding = GetAcceptEncoding(request)]
								   {
									   auto response = EncodeContent(std::invoke(invoker, *m_requester, std::cref(root), QString(NAVIGATION_STARTS).arg(root, key, value), value.toUpper()), acceptEncoding);
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(QString(AUTHOR_LIST).arg(root, key, ARG),
			               [this, root, key, invoker = navigationAuthorsInvoker](const QString& navigationId, const QHttpServerRequest& request)
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker, navigationId, acceptEncoding = GetAcceptEncoding(request)]
								   {
									   auto response = EncodeContent(std::invoke(invoker, *m_requester, std::cref(root), QString(AUTHOR_LIST).arg(root, key, navigationId), navigationId.toUpper(), QString {}),
					                                                 acceptEncoding);
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(QString(BOOK_LIST).arg(root, key, ARG),
			               [this, root, key, invoker = authorBooksInvoker](const QString& navigationId, const QHttpServerRequest& request)
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker, navigationId, acceptEncoding = GetAcceptEncoding(request)]
								   {
									   auto response =
										   EncodeContent(std::invoke(invoker, *m_requester, std::cref(root), QString(AUTHOR_LIST).arg(root, key, navigationId), navigationId.toUpper(), QString {}, QString {}),
					                                     acceptEncoding);
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(QString(NAVIGATION_AUTHOR_STARTS).arg(root, key, ARG, ARG),
			               [this, root, key, invoker = navigationAuthorsInvoker](const QString& navigationId, const QString& value, const QHttpServerRequest& request)
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker, navigationId, value, acceptEncoding = GetAcceptEncoding(request)]
								   {
									   auto response = EncodeContent(
										   std::invoke(invoker, *m_requester, std::cref(root), QString(NAVIGATION_AUTHOR_STARTS).arg(root, key, navigationId, value), navigationId.toUpper(), value.toUpper()),
										   acceptEncoding);
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(QString(NAVIGATION_AUTHOR).arg(root, key, ARG, ARG),
			               [this, root, key, invoker = authorBooksInvoker](const QString& navigationId, const QString& authorId, const QHttpServerRequest& request)
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker, navigationId, authorId, acceptEncoding = GetAcceptEncoding(request)]
								   {
									   auto response = EncodeContent(std::invoke(invoker,
					                                                             *m_requester,
					                                                             std::cref(root),
					                                                             QString(NAVIGATION_AUTHOR_STARTS).arg(root, key, navigationId, authorId),
					                                                             navigationId.toUpper(),
					                                                             authorId.toUpper(),
					                                                             QString {}),
					                                                 acceptEncoding);
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(QString(NAVIGATION_AUTHOR_BOOKS_STARTS).arg(root, key, ARG, ARG, ARG),
			               [this, root, key, invoker = authorBooksInvoker](const QString& navigationId, const QString& authorId, const QString& value, const QHttpServerRequest& request)
			               {
							   return QtConcurrent::run(
								   [this, root, key, invoker, navigationId, authorId, value, acceptEncoding = GetAcceptEncoding(request)]
								   {
									   auto response = EncodeContent(std::invoke(invoker,
					                                                             *m_requester,
					                                                             std::cref(root),
					                                                             QString(NAVIGATION_AUTHOR_STARTS).arg(root, key, navigationId, authorId),
					                                                             navigationId.toUpper(),
					                                                             authorId.toUpper(),
					                                                             value),
					                                                 acceptEncoding);
									   SetContentType(response, root, MessageType::Atom);
									   return response;
								   });
						   });

			m_server.route(
				QString(BOOK_LIST_STARTS).arg(root, key, ARG, ARG),
				[this, root, key, invoker = authorBooksInvoker](const QString& navigationId, const QString& value, const QHttpServerRequest& request)
				{
					return QtConcurrent::run(
						[this, root, key, invoker, navigationId, value, acceptEncoding = GetAcceptEncoding(request)]
						{
							auto response = EncodeContent(
								std::invoke(invoker, *m_requester, std::cref(root), QString(BOOK_LIST_STARTS).arg(root, key, navigationId, value), navigationId.toUpper(), QString {}, value.toUpper()),
								acceptEncoding);
							SetContentType(response, root, MessageType::Atom);
							return response;
						});
				});
		}
	}

	void RouteReactApp()
	{
		m_server.route(
			"/",
			[this](const QHttpServerRequest& request)
			{
				return QtConcurrent::run(
					[this, acceptEncoding = GetAcceptEncoding(request)]
					{ return *FromWebsite("index.html", acceptEncoding, [this](QByteArray data) { return data.replace("###Collection###", m_collectionProvider->GetActiveCollection().name.toUtf8()); }); });
			});

		m_server.route(QString(ASSETS).arg(ARG),
		               [this](const QString& fileName, const QHttpServerRequest& request)
		               { return QtConcurrent::run([this, fileName, acceptEncoding = GetAcceptEncoding(request)] { return *FromWebsite("assets/" + fileName, acceptEncoding); }); });

		using Requester = QByteArray (IReactAppRequester::*)(const QString&) const;
		static constexpr std::tuple<const char*, const char*, Requester> booksApiDescription[] {
#define OPDS_GET_BOOKS_API_ITEM(NAME, QUERY) { #NAME, #QUERY, &IReactAppRequester::NAME },
			OPDS_GET_BOOKS_API_ITEMS_X_MACRO
#undef OPDS_GET_BOOKS_API_ITEM
		};

		for (const auto& [name, queryKey, requester] : booksApiDescription)
		{
			m_server.route(QString(GET_BOOKS_API).arg(name),
			               [this, requester, queryKey](const QHttpServerRequest& request)
			               {
							   return QtConcurrent::run(
								   [this, requester, value = request.query().queryItemValue(queryKey, QUrl::FullyDecoded), acceptEncoding = GetAcceptEncoding(request)]
								   {
									   auto response = EncodeContent(std::invoke(requester, *m_reactAppRequester, std::cref(value)), acceptEncoding);
									   ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/json");
									   return response;
								   });
						   });
		}
		m_server.route(QString(GET_BOOKS_API_COVER).arg(ARG),
		               [this](const QString& value)
		               {
						   return QtConcurrent::run(
							   [this, value]
							   {
								   QHttpServerResponse response(m_noSqlRequester->GetCover(value));
								   ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "image/jpeg");
								   return response;
							   });
					   });

		m_server.route(QString(GET_BOOKS_API_BOOK_DATA_COMPACT).arg(ARG),
		               [this](const QString& value, const QHttpServerRequest& request) { return GetBook(&INoSqlRequester::GetBook, value, GetAcceptEncoding(request), "application/fb2", false); });

		m_server.route(QString(GET_BOOKS_API_BOOK_DATA).arg(ARG),
		               [this](const QString& value, const QHttpServerRequest& request) { return GetBook(&INoSqlRequester::GetBook, value, GetAcceptEncoding(request), "application/fb2", true); });

		m_server.route(QString(GET_BOOKS_API_BOOK_ZIP).arg(ARG), [this](const QString& value) { return GetBook(&INoSqlRequester::GetBookZip, value, "", "application/zip", true); });
	}

private:
	QLocalServer m_localServer;
	QHttpServer m_server;
	std::shared_ptr<const Flibrary::ICollectionProvider> m_collectionProvider;
	std::shared_ptr<const IRequester> m_requester;
	std::shared_ptr<const IReactAppRequester> m_reactAppRequester;
	std::shared_ptr<const INoSqlRequester> m_noSqlRequester;
};

Server::Server(const std::shared_ptr<const ISettings>& settings,
               std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
               std::shared_ptr<const IRequester> requester,
               std::shared_ptr<const IReactAppRequester> reactAppRequester,
               std::shared_ptr<const INoSqlRequester> noSqlRequester)
	: m_impl(*settings, std::move(collectionProvider), std::move(requester), std::move(reactAppRequester), std::move(noSqlRequester))
{
	PLOGV << "Server created";
}

Server::~Server()
{
	PLOGV << "Server destroyed";
}
