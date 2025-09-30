#include "Connection.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "log.h"

using namespace HomeCompa::RestAPI;
using namespace QtLib;

struct Connection::Impl
{
	QNetworkAccessManager networkManager;

	explicit Impl(const IConnection& self)
		: m_self(self)
	{
	}

	QNetworkRequest PrepareRequest() const
	{
		QNetworkRequest request;

		for (const auto& [k, v] : m_self.GetHeaders())
			request.setRawHeader(k.c_str(), v.c_str());

		request.setRawHeader("User-Agent", "github_api/1.0");

		return request;
	}

private:
	const IConnection& m_self;
};

Connection::Connection(std::string address, Headers headers)
	: BaseConnection(std::move(address), std::move(headers))
	, m_impl(*this)
{
}

Connection::~Connection() = default;

IConnection::Headers Connection::GetPage(const std::string& page)
{
	QNetworkRequest request = m_impl->PrepareRequest();
	const QUrl      url(QString::fromStdString(page));
	request.setUrl(url);

	QNetworkReply* reply = m_impl->networkManager.get(request);

	QEventLoop eventLoop;

	Headers headers;

	QObject::connect(
		reply,
		&QNetworkReply::readChannelFinished,
		&m_impl->networkManager,
		[&, reply]() {
			const QByteArray rawData = reply->readAll();

			QJsonParseError error;
			const auto      doc = QJsonDocument::fromJson(rawData, &error);
			if (error.error == QJsonParseError::NoError)
				OnDataReceived(doc);

			for (const auto& [name, value] : reply->rawHeaderPairs())
				headers.try_emplace(QString::fromUtf8(name).toLower().simplified().toStdString(), QString::fromUtf8(value).simplified().toStdString());

			reply->deleteLater();

			eventLoop.exit();
		},
		Qt::DirectConnection
	);

	QObject::connect(reply, &QNetworkReply::errorOccurred, [reply, &page](const QNetworkReply::NetworkError code) {
		PLOGE << QString("Error (%1 - %2) occurred when processing request %3").arg(code).arg(reply->errorString()).arg(page.c_str());
	});

	QObject::connect(reply, &QNetworkReply::sslErrors, [&page](const QList<QSslError>& errors) {
		PLOGE << QString("Ssl errors occurred when processing request %1:").arg(page.c_str());

		for (const auto& error : std::as_const(errors))
			PLOGE << error.errorString();
	});

	eventLoop.exec();

	return headers;
}
