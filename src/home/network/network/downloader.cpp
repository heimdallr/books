#include "downloader.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "log.h"

using namespace HomeCompa::Network;

class Downloader::Impl
{
public:
	Impl()
	{
		QObject::connect(&m_manager, &QNetworkAccessManager::finished, &m_manager, [](QNetworkReply* reply) {
			reply->deleteLater();
		});
	}

public:
	size_t Download(const QString& url, QIODevice& io, OnFinish callback, OnProgress progress, const QHttpHeaders& headers)
	{
		QNetworkRequest request(url);
		if (!headers.isEmpty())
			request.setHeaders(headers);

		auto*      reply = m_manager.get(request);
		const auto id    = ++m_id;
		m_replies.try_emplace(reply, std::make_tuple(id, QNetworkReply::NetworkError::NoError, QString {}));

		QObject::connect(reply, &QObject::destroyed, &m_manager, [&, callback = std::move(callback)](const QObject* obj) {
			const auto it = m_replies.find(obj);
			assert(it != m_replies.end());
			const auto& [idMessage, code, message] = it->second;
			callback(idMessage, code, message);
			m_replies.erase(obj);
		});

		QObject::connect(reply, &QNetworkReply::readyRead, &m_manager, [&, reply] {
			io.write(reply->readAll());
		});

		QObject::connect(reply, &QNetworkReply::errorOccurred, &m_manager, [&, reply, url](const QNetworkReply::NetworkError code) {
			auto error = reply->errorString();
			PLOGE << QString("Download '%1' error: %2 %3").arg(url).arg(static_cast<int>(code)).arg(error);
			const auto it = m_replies.find(reply);
			assert(it != m_replies.end());
			auto& [_, errorCode, errorMessage] = it->second;
			errorCode                          = code;
			errorMessage                       = std::move(error);
			reply->deleteLater();
		});

		if (progress)
			QObject::connect(reply, &QNetworkReply::downloadProgress, &m_manager, [reply, progress = std::move(progress)](const qint64 bytesReceived, const qint64 bytesTotal) {
				bool stopped = false;
				progress(bytesReceived, bytesTotal, stopped);
				if (stopped)
					reply->close();
			});

		return id;
	}

private:
	size_t                                                                                       m_id { 0 };
	std::unordered_map<const QObject*, std::tuple<size_t, QNetworkReply::NetworkError, QString>> m_replies;
	QNetworkAccessManager                                                                        m_manager;
};

Downloader::Downloader()
{
	PLOGV << "Downloader created";
}

Downloader::~Downloader()
{
	PLOGV << "Downloader destroyed";
}

size_t Downloader::Download(const QString& url, QIODevice& io, OnFinish callback, OnProgress progress, const QHttpHeaders& headers)
{
	return m_impl->Download(url, io, std::move(callback), std::move(progress), headers);
}
