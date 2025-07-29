#include "BaseJokeRequester.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "fnd/ScopedCall.h"

#include "network/network/downloader.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

BaseJokeRequester::Item::Item(std::weak_ptr<IClient> client)
	: client { std::move(client) }
{
	stream.open(QIODevice::WriteOnly);
}

struct BaseJokeRequester::Impl
{
	std::shared_ptr<Network::Downloader> downloader;
	const QString uri;
	QHttpHeaders headers;
	std::unordered_map<size_t, std::unique_ptr<Item>> requests;
};

BaseJokeRequester::BaseJokeRequester(std::shared_ptr<Network::Downloader> downloader, QString uri, QHttpHeaders headers)
	: m_impl { std::move(downloader), std::move(uri), std::move(headers) }
{
}

BaseJokeRequester::~BaseJokeRequester() = default;

void BaseJokeRequester::Request(std::weak_ptr<IClient> client)
{
	auto item = std::make_unique<Item>(std::move(client));
	const auto id =
		m_impl->downloader->Download(m_impl->uri, item->stream, [this](const size_t idMessage, const int code, const QString& message) { OnResponse(idMessage, code, message); }, {}, m_impl->headers);
	m_impl->requests.try_emplace(id, std::move(item));
}

void BaseJokeRequester::OnResponse(const size_t id, const int code, const QString& message)
{
	const auto it = m_impl->requests.find(id);
	assert(it != m_impl->requests.end());
	const ScopedCall requestGuard([&] { m_impl->requests.erase(it); });

	const auto client = it->second->client.lock();
	if (!client)
		return;

	auto errorMessage = message;
	const ScopedCall errorGuard(
		[this, &errorMessage, client]
		{
			if (errorMessage.isEmpty())
				return;

			PLOGE << errorMessage;
			client->OnTextReceived(QString(R"(<p>%1 failed:</p><p style="color:Red;">%2</p>)").arg(m_impl->uri, errorMessage));
		});

	if (code != QNetworkReply::NetworkError::NoError)
		return;

	if (Process(it->second->data, client))
		return;

	QJsonParseError parseError;
	const auto doc = QJsonDocument::fromJson(it->second->data, &parseError);
	if (parseError.error != QJsonParseError::NoError)
		return (void)(errorMessage = parseError.errorString());

	if (const auto value = doc.isObject() ? QJsonValue { doc.object() } : doc.isArray() ? QJsonValue { doc.array() } : QJsonValue {}; Process(value, client))
		return;

	errorMessage = QString::fromUtf8(it->second->data); //-V1001
}

bool BaseJokeRequester::Process(const QJsonValue&, std::weak_ptr<IClient>)
{
	assert(false && "unexpected call");
	return false;
}

bool BaseJokeRequester::Process(const QByteArray&, std::weak_ptr<IClient>)
{
	return false;
}
