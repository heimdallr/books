#include "BaseJokeRequester.h"

#include <QJsonArray>
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
	QString uri;
	std::unordered_map<size_t, std::unique_ptr<Item>> requests;
	Network::Downloader downloader;
};

BaseJokeRequester::BaseJokeRequester(QString uri)
	: m_impl { std::move(uri) }
{
}

BaseJokeRequester::~BaseJokeRequester() = default;

void BaseJokeRequester::SetHeaders(QHttpHeaders headers)
{
	m_impl->downloader.SetHeaders(std::move(headers));
}

void BaseJokeRequester::Request(std::weak_ptr<IClient> client)
{
	auto item = std::make_unique<Item>(std::move(client));
	const auto id = m_impl->downloader.Download(m_impl->uri, item->stream, [this](const size_t idMessage, const int code, const QString& message) { OnResponse(idMessage, code, message); });
	m_impl->requests.try_emplace(id, std::move(item));
}

void BaseJokeRequester::OnResponse(const size_t id, const int code, const QString&)
{
	const auto it = m_impl->requests.find(id);
	assert(it != m_impl->requests.end());
	const ScopedCall requestGuard([&] { m_impl->requests.erase(it); });

	if (code != QNetworkReply::NetworkError::NoError)
		return;

	const auto client = it->second->client.lock();
	if (!client)
		return;

	if (Process(it->second->data, client))
		return;

	QJsonParseError parseError;
	const auto doc = QJsonDocument::fromJson(it->second->data, &parseError);
	if (parseError.error != QJsonParseError::NoError)
	{
		PLOGE << parseError.errorString();
		return;
	}

	const auto value = doc.isObject() ? QJsonValue { doc.object() } : doc.isArray() ? QJsonValue { doc.array() } : QJsonValue {};
	if (!Process(value, client))
		PLOGE << "unexpected response: " << QString::fromUtf8(it->second->data);
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
