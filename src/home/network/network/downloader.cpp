#include "downloader.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "log.h"

using namespace HomeCompa::Network;

class Downloader::Impl
{
public:
	void Download(const QString& url, QIODevice& io, OnFinish callback, OnProgress progress)
	{
		PLOGD << "Download started: " << url;

		const QNetworkRequest request(url);
		auto* reply = m_manager.get(request);
		m_replies.try_emplace(reply, std::make_pair(QNetworkReply::NetworkError::NoError, QString {}));

		QObject::connect(reply,
		                 &QObject::destroyed,
		                 &m_manager,
		                 [&, callback = std::move(callback)](const QObject* obj)
		                 {
							 const auto it = m_replies.find(obj);
							 assert(it != m_replies.end());
							 callback(it->second.first, it->second.second);
						 });

		QObject::connect(&m_manager,
		                 &QNetworkAccessManager::finished,
		                 &m_manager,
		                 [reply, url]
		                 {
							 PLOGD << "Download finished: " << url;
							 reply->deleteLater();
						 });

		QObject::connect(reply, &QNetworkReply::readyRead, &m_manager, [&, reply] { io.write(reply->readAll()); });

		QObject::connect(reply,
		                 &QNetworkReply::errorOccurred,
		                 &m_manager,
		                 [&, reply, url](const QNetworkReply::NetworkError code)
		                 {
							 auto error = reply->errorString();
							 PLOGE << "Download error: " << url;
							 PLOGE << QString("(%1) %2").arg(static_cast<int>(code)).arg(error);
							 const auto it = m_replies.find(reply);
							 assert(it != m_replies.end());
							 it->second = std::make_pair(code, std::move(error));
							 reply->deleteLater();
						 });

		if (progress)
		{
			QObject::connect(reply,
			                 &QNetworkReply::downloadProgress,
			                 &m_manager,
			                 [reply, progress = std::move(progress)](const qint64 bytesReceived, const qint64 bytesTotal)
			                 {
								 bool stopped = false;
								 progress(bytesReceived, bytesTotal, stopped);
								 if (stopped)
									 reply->close();
							 });
		}
	}

private:
	QNetworkAccessManager m_manager;
	std::unordered_map<const QObject*, std::pair<QNetworkReply::NetworkError, QString>> m_replies;
};

Downloader::Downloader()
{
	PLOGV << "Downloader created";
}

Downloader::~Downloader()
{
	PLOGV << "Downloader destroyed";
}

void Downloader::Download(const QString& url, QIODevice& io, OnFinish callback, OnProgress progress)
{
	m_impl->Download(url, io, std::move(callback), std::move(progress));
}
