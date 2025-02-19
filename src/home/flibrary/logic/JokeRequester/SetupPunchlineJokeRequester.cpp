#include "SetupPunchlineJokeRequester.h"

#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QString>

#include "fnd/ScopedCall.h"

#include "network/network/downloader.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

namespace
{
struct Item
{
	std::weak_ptr<IJokeRequester::IClient> client;
	QByteArray data;
	QBuffer stream { &data };

	explicit Item(std::weak_ptr<IJokeRequester::IClient> client)
		: client { std::move(client) }
	{
		stream.open(QIODevice::WriteOnly);
	}
};
}

struct SetupPunchlineJokeRequester::Impl
{
	Network::Downloader downloader;
	std::unordered_map<size_t, std::unique_ptr<Item>> requests;
};

SetupPunchlineJokeRequester::SetupPunchlineJokeRequester() = default;
SetupPunchlineJokeRequester::~SetupPunchlineJokeRequester() = default;

void SetupPunchlineJokeRequester::Request(std::weak_ptr<IClient> client)
{
	auto item = std::make_unique<Item>(std::move(client));
	const auto id = m_impl->downloader.Download("https://official-joke-api.appspot.com/random_joke",
	                                            item->stream,
	                                            [this](const size_t idMessage, const int code, const QString& message) { OnResponse(idMessage, code, message); });
	m_impl->requests.try_emplace(id, std::move(item));
}

void SetupPunchlineJokeRequester::OnResponse(const size_t id, const int code, const QString& message)
{
	const auto it = m_impl->requests.find(id);
	assert(it != m_impl->requests.end());
	const ScopedCall requestGuard([&] { m_impl->requests.erase(it); });

	if (code != QNetworkReply::NetworkError::NoError)
		PLOGE << message;

	auto client = it->second->client.lock();
	if (!client)
		return;

	QJsonParseError parseError;
	const auto doc = QJsonDocument::fromJson(it->second->data, &parseError);
	if (parseError.error != QJsonParseError::NoError)
	{
		PLOGE << parseError.errorString();
		return;
	}

	if (!doc.isObject())
	{
		PLOGE << "unexpected response: " << QString::fromUtf8(it->second->data);
		return;
	}

	const auto jsonObject = doc.object();

	client->Response(QString("<p>- %1</p><p>- %2</p>").arg(jsonObject["setup"].toString(), jsonObject["punchline"].toString()));
}
