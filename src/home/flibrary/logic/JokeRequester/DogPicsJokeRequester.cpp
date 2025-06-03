#include "DogPicsJokeRequester.h"

#include <QJsonObject>
#include <QNetworkReply>

#include "fnd/ScopedCall.h"

#include "network/network/downloader.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

struct DogPicsJokeRequester::Impl
{
	std::unordered_map<size_t, std::unique_ptr<Item>> requests;
	Network::Downloader downloader;
};

DogPicsJokeRequester::DogPicsJokeRequester()
	: BaseJokeRequester("http://dog.ceo/api/breeds/image/random")
{
}

DogPicsJokeRequester::~DogPicsJokeRequester() = default;

bool DogPicsJokeRequester::Process(const QJsonValue& value, const std::weak_ptr<IClient>& client)
{
	const auto jsonObject = value.toObject();
	const auto uri = jsonObject["message"];
	if (!uri.isString())
		return false;

	auto item = std::make_unique<Item>(client);
	const auto id = m_impl->downloader.Download(uri.toString(), item->stream, [this](const size_t idMessage, const int code, const QString& message) { OnImageReceived(idMessage, code, message); });
	m_impl->requests.try_emplace(id, std::move(item));
	return true;
}

void DogPicsJokeRequester::OnImageReceived(const size_t id, const int code, const QString& message)
{
	const auto it = m_impl->requests.find(id);
	assert(it != m_impl->requests.end());
	const ScopedCall requestGuard([&] { m_impl->requests.erase(it); });

	if (code != QNetworkReply::NetworkError::NoError)
		PLOGE << message;

	if (const auto client = it->second->client.lock())
		client->OnImageReceived(it->second->data);
}
