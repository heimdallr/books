#include "Server.h"

#include <ranges>

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
#include "interface/localization.h"

#include "util/ISettings.h"
#include "util/hash.h"

#include "log.h"
#include "root.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Opds;

namespace
{

constexpr auto ARG        = "<arg>";
constexpr auto FAVICON    = "/favicon.ico";
constexpr auto OPENSEARCH = "/opds/opensearch";

constexpr auto GET_BOOKS_API                   = "/main/getBooks/%1";
constexpr auto GET_BOOKS_API_ASSETS            = "/assets/%1";
constexpr auto GET_BOOKS_API_COVER             = "/Images/covers/%1";
constexpr auto GET_BOOKS_API_BOOK_DATA         = "/Images/fb2/%1";
constexpr auto GET_BOOKS_API_BOOK_ZIP          = "/Images/zip/%1";
constexpr auto GET_BOOKS_API_BOOK_DATA_COMPACT = "/Images/fb2compact/%1";

const auto CONTEXT       = "Http";
const auto AUTH_REQUIRED = QT_TRANSLATE_NOOP("Http", "Authentication required");
TR_DEF

#define OPDS_REQUEST_ROOT_ITEM(NAME) constexpr auto(NAME) = "/" #NAME;
OPDS_REQUEST_ROOT_ITEMS_X_MACRO
#undef OPDS_REQUEST_ROOT_ITEM

bool AuthCheckerStub(const QString& expected, const QByteArray& received)
{
	return Util::GetSaltedHash(QString::fromUtf8(received)) == expected;
}

bool AuthCheckerSimple(const QString& expected, const QByteArray& received)
{
	const auto decoded = QString::fromUtf8(QByteArray::fromBase64(received));
	return Util::GetSaltedHash(decoded) == expected;
}

bool AuthCheckerBasic(const QString& expected, const QByteArray& received)
{
	auto receivedStr = QString::fromUtf8(received);
	if (!receivedStr.startsWith("basic", Qt::CaseInsensitive))
		return false;

	receivedStr = receivedStr.last(receivedStr.length() - 5).simplified();
	return AuthCheckerSimple(expected, receivedStr.toUtf8());
}

using AuthChecker = bool (*)(const QString& expected, const QByteArray& received);

constexpr AuthChecker AUTH_CHECKERS[] {
	&AuthCheckerStub,
	&AuthCheckerSimple,
	&AuthCheckerBasic,
};

void ReplaceOrAppendHeader(QHttpServerResponse& response, const QHttpHeaders::WellKnownHeader key, const QString& value)
{
	auto                        h  = response.headers();
	[[maybe_unused]] const auto ok = h.replaceOrAppend(key, value);
	assert(ok);
	response.setHeaders(std::move(h));
}

void ReplaceOrAppendHeader(QHttpServerResponse& response, const QString& key, const QString& value)
{
	auto                        h  = response.headers();
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
	const auto  rootIndex   = std::distance(std::begin(ROOTS), std::ranges::find_if(ROOTS, [root = root.toStdString()](const char* item) {
                                             return root == item;
                                         }));
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
		QBuffer          stream(&zipped);
		const ScopedCall streamGuard(
			[&] {
				stream.open(QIODevice::WriteOnly);
			},
			[&] {
				stream.close();
			}
		);
		Zip zip(stream, Zip::Format::GZip);
		zip.SetProperty(Zip::PropertyId::CompressionMethod, QVariant::fromValue(Zip::CompressionMethod::Deflate));
		zip.SetProperty(Zip::PropertyId::CompressionLevel, QVariant::fromValue(Zip::CompressionLevel::Fast));
		auto zipFiles = Zip::CreateZipFileController();
		zipFiles->AddFile("file", std::move(src));
		zip.Write(std::move(zipFiles));
	}

	QHttpServerResponse result { zipped };
	ReplaceOrAppendHeader(result, QHttpHeaders::WellKnownHeader::ContentEncoding, "gzip");
	return result;
}

std::optional<QHttpServerResponse> FromFile(
	const QString&                               fileName,
	const QString&                               acceptEncoding,
	const QString&                               contentType,
	const std::function<QByteArray(QByteArray)>& dataUpdater =
		[](QByteArray data) {
			return data;
		}
)
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

std::optional<QHttpServerResponse> FromWebsite(
	const QString&                               fileName,
	const QString&                               acceptEncoding,
	const std::function<QByteArray(QByteArray)>& dataUpdater =
		[](QByteArray data) {
			return data;
		}
)
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

template <typename T>
T GetParameters(const QHttpServerRequest& request)
{
	T result;
	std::ranges::transform(request.query().queryItems(QUrl::FullyDecoded), std::inserter(result, result.end()), [](const auto& item) {
		return std::make_pair(item.first, item.second);
	});
	return result;
}

template <typename T>
T GetParameters(const QString& data)
{
	T result;
	std::ranges::transform(
		data.split('&', Qt::SkipEmptyParts) | std::views::filter([](const auto& item) {
			return item.contains('=');
		}),
		std::inserter(result, result.end()),
		[](const auto& item) {
			const auto values = item.split('=');
			return std::make_pair(QUrl::fromPercentEncoding(values.front().toUtf8()), QUrl::fromPercentEncoding(values.back().toUtf8()));
		}
	);
	return result;
}

auto Authenticate()
{
	return QtConcurrent::run([] {
		QHttpServerResponse response { AUTH_REQUIRED, QHttpServerResponder::StatusCode::Unauthorized };
		auto                h = response.headers();
		h.append(QHttpHeaders::WellKnownHeader::WWWAuthenticate, R"(Basic realm="Restricted Area")");
		response.setHeaders(std::move(h));
		return response;
	});
}

} // namespace

class Server::Impl
{
	using AuthorizationAllowFunctor = std::function<QHttpServerResponse(const IRequester::Parameters&, const QString&)>;

public:
	Impl(
		std::shared_ptr<const ISettings>                     settings,
		std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
		std::shared_ptr<const IRequester>                    requester,
		std::shared_ptr<const IReactAppRequester>            reactAppRequester,
		std::shared_ptr<const INoSqlRequester>               noSqlRequester
	)
		: m_settings { std::move(settings) }
		, m_collectionProvider { std::move(collectionProvider) }
		, m_requester { std::move(requester) }
		, m_reactAppRequester { std::move(reactAppRequester) }
		, m_noSqlRequester { std::move(noSqlRequester) }
	{
		const auto host = [this]() -> QHostAddress {
			const auto address = m_settings->Get(Flibrary::Constant::Settings::OPDS_HOST_KEY, Flibrary::Constant::Settings::OPDS_HOST_DEFAULT);
			return address == Flibrary::Constant::Settings::OPDS_HOST_DEFAULT ? QHostAddress::Any : QHostAddress { address };
		}();
		InitHttp(host, static_cast<uint16_t>(m_settings->Get(Flibrary::Constant::Settings::OPDS_PORT_KEY, Flibrary::Constant::Settings::OPDS_PORT_DEFAULT)));

		if (!m_localServer.listen(Flibrary::Constant::OPDS_SERVER_NAME))
			throw std::runtime_error(std::format("Cannot listen pipe {}", Flibrary::Constant::OPDS_SERVER_NAME));

		QObject::connect(&m_localServer, &QLocalServer::newConnection, [this] {
			auto* socket = m_localServer.nextPendingConnection();
			QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
			QObject::connect(socket, &QIODevice::readyRead, [socket] {
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
		return QtConcurrent::run([this, getter, value = std::move(value), acceptEncoding = std::move(acceptEncoding), contentType = std::move(contentType), restoreImages] {
			auto [fileName, body] = std::invoke(getter, *m_noSqlRequester, std::cref(value), restoreImages);
			auto response         = EncodeContent(std::move(body), acceptEncoding);
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

		m_server.addAfterRequestHandler(&m_server, [this](const QHttpServerRequest& request, QHttpServerResponse& resp) {
			const auto log = QString("%1 requests %2%3").arg(request.remoteAddress().toString(), request.url().path(), request.query().isEmpty() ? "" : QString("?%1").arg(request.query().toString()));
			PLOGD << log;

			ReplaceOrAppendHeader(resp, QHttpHeaders::WellKnownHeader::Server, "FLibrary HTTP Server");
			ReplaceOrAppendHeader(resp, QHttpHeaders::WellKnownHeader::Date, QDateTime::currentDateTime().toUTC().toString("ddd, dd MMM yyyy hh:mm:ss") + " GMT");
			ReplaceOrAppendHeader(resp, QHttpHeaders::WellKnownHeader::Connection, "keep-alive");
			ReplaceOrAppendHeader(resp, QHttpHeaders::WellKnownHeader::KeepAlive, "timeout=5");
		});

		Route(host, port);
	}

	void Route(const QHostAddress& host, const uint16_t port)
	{
		m_server.route(FAVICON, [this] {
			return QtConcurrent::run([this] {
				auto response = *FromFile(":/icons/main.ico", "", "image/x-icon");
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "image/x-icon");
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::CacheControl, "public, max-age=0");
				return response;
			});
		});

		std::vector<QString> roots;

		if (m_settings->Get(Flibrary::Constant::Settings::OPDS_OPDS_ENABLED, true))
			roots.emplace_back(opds);

		if (m_settings->Get(Flibrary::Constant::Settings::OPDS_WEB_ENABLED, true))
			roots.emplace_back(web);

		m_server.route(OPENSEARCH, [host = host.toString(), port] {
			return QString(R"(<Url type="application/atom+xml;profile=opds-catalog" xmlns:atom="http://www.w3.org/2005/Atom" template="http://%1:%2/search?q={searchTerms}" />)").arg(host).arg(port);
		});

		for (const auto& root : roots)
			RouteWithRoot(root);

		if (m_settings->Get(Flibrary::Constant::Settings::OPDS_REACT_APP_ENABLED, true))
			RouteReactApp();
	}

	void RouteWithRoot(const QString& root)
	{
		using Invoker = QByteArray (IRequester::*)(const QString&, const IRequester::Parameters&) const;
		static constexpr std::tuple<const char* /*path*/, Invoker> descriptions[] {
#define OPDS_INVOKER_ITEM(NAME) { #NAME, &IRequester::Get##NAME },
			OPDS_NAVIGATION_ITEMS_X_MACRO OPDS_ADDITIONAL_ITEMS_X_MACRO
#undef OPDS_INVOKER_ITEM
			{  nullptr,     &IRequester::GetRoot },
			{ "search",      &IRequester::Search },
			{   "read", &IRequester::GetBookText },
		};

		for (const auto& [path, invoker] : descriptions)
		{
			const auto pathPattern = QString("%1%2").arg(root).arg(path ? QString("/%1").arg(path) : QString {});
			m_server.route(pathPattern, [this, root, invoker](const QHttpServerRequest& request) {
				return Authorization(request, web, [this, root, invoker](const IRequester::Parameters& parameters, const QString& acceptEncoding) {
					auto response = EncodeContent(std::invoke(invoker, *m_requester, std::cref(root), std::cref(parameters)), acceptEncoding);
					SetContentType(response, root, MessageType::Atom);
					return response;
				});
			});
		}
	}

	void RouteReactApp()
	{
		m_server.route("/", [this](const QHttpServerRequest& request) {
			return Authorization(request, "/", [this](const IRequester::Parameters&, const QString& acceptEncoding) {
				return *FromWebsite("index.html", acceptEncoding, [this](QByteArray data) {
					return data.replace("###Collection###", m_collectionProvider->GetActiveCollection().name.toUtf8());
				});
			});
		});

		m_server.route(QString(GET_BOOKS_API_ASSETS).arg(ARG), [this](const QString& fileName, const QHttpServerRequest& request) {
			return QtConcurrent::run([this, fileName, acceptEncoding = GetAcceptEncoding(request)] {
				return *FromWebsite("assets/" + fileName, acceptEncoding);
			});
		});

		using Requester = QByteArray (IReactAppRequester::*)(const std::unordered_map<QString, QString>&) const;
		static constexpr std::tuple<const char*, Requester> booksApiDescription[] {
#define OPDS_GET_BOOKS_API_ITEM(NAME) { #NAME, &IReactAppRequester::NAME },
			OPDS_GET_BOOKS_API_ITEMS_X_MACRO
#undef OPDS_GET_BOOKS_API_ITEM
		};

		for (const auto& [name, requester] : booksApiDescription)
		{
			m_server.route(QString(GET_BOOKS_API).arg(name), [this, requester](const QHttpServerRequest& request) {
				return QtConcurrent::run([this, requester, parameters = GetParameters<IReactAppRequester::Parameters>(request), acceptEncoding = GetAcceptEncoding(request)] {
					auto response = EncodeContent(std::invoke(requester, *m_reactAppRequester, std::cref(parameters)), acceptEncoding);
					ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "application/json");
					return response;
				});
			});
		}
		m_server.route(QString(GET_BOOKS_API_COVER).arg(ARG), [this](const QString& value) {
			return QtConcurrent::run([this, value] {
				QHttpServerResponse response(m_noSqlRequester->GetCover(value));
				ReplaceOrAppendHeader(response, QHttpHeaders::WellKnownHeader::ContentType, "image/jpeg");
				return response;
			});
		});

		m_server.route(QString(GET_BOOKS_API_BOOK_DATA_COMPACT).arg(ARG), [this](const QString& value, const QHttpServerRequest& request) {
			return GetBook(&INoSqlRequester::GetBook, value, GetAcceptEncoding(request), "application/fb2", false);
		});

		m_server.route(QString(GET_BOOKS_API_BOOK_DATA).arg(ARG), [this](const QString& value, const QHttpServerRequest& request) {
			return GetBook(&INoSqlRequester::GetBook, value, GetAcceptEncoding(request), "application/fb2", true);
		});

		m_server.route(QString(GET_BOOKS_API_BOOK_ZIP).arg(ARG), [this](const QString& value) {
			return GetBook(&INoSqlRequester::GetBookZip, value, "", "application/zip", true);
		});
	}

	QFuture<QHttpServerResponse> Authorization(const QHttpServerRequest& request, QString url, AuthorizationAllowFunctor allow) const
	{
		auto       acceptEncoding = GetAcceptEncoding(request);
		const auto expectedAuth   = m_settings->Get(Flibrary::Constant::Settings::OPDS_AUTH, QString {});
		auto       parameters     = GetParameters<IRequester::Parameters>(request);

		if (expectedAuth.isEmpty())
			return QtConcurrent::run([this, allow = std::move(allow), acceptEncoding = std::move(acceptEncoding), parameters = std::move(parameters)] {
				return allow(parameters, acceptEncoding);
			});

		const auto& headers       = request.headers();
		const auto  authorization = headers.value("authorization").toByteArray();
		if (authorization.isEmpty())
		{
			PLOGW << "Attempt connection without authorization";
			return Authenticate();
		}

		if (std::ranges::none_of(AUTH_CHECKERS, [&](const auto checker) {
				return checker(expectedAuth, authorization);
			}))
		{
			PLOGW << "Attempt connection with wrong authorization: " << authorization;
			return Authenticate();
		}

		return QtConcurrent::run([this, url = std::move(url), allow = std::move(allow), parameters = std::move(parameters), acceptEncoding = std::move(acceptEncoding)]() mutable {
			return allow(parameters, acceptEncoding);
		});
	}

private:
	QLocalServer                                         m_localServer;
	QHttpServer                                          m_server;
	std::shared_ptr<const ISettings>                     m_settings;
	std::shared_ptr<const Flibrary::ICollectionProvider> m_collectionProvider;
	std::shared_ptr<const IRequester>                    m_requester;
	std::shared_ptr<const IReactAppRequester>            m_reactAppRequester;
	std::shared_ptr<const INoSqlRequester>               m_noSqlRequester;
};

Server::Server(
	std::shared_ptr<const ISettings>                     settings,
	std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
	std::shared_ptr<const IRequester>                    requester,
	std::shared_ptr<const IReactAppRequester>            reactAppRequester,
	std::shared_ptr<const INoSqlRequester>               noSqlRequester
)
	: m_impl(std::move(settings), std::move(collectionProvider), std::move(requester), std::move(reactAppRequester), std::move(noSqlRequester))
{
	PLOGV << "Server created";
}

Server::~Server()
{
	PLOGV << "Server destroyed";
}
