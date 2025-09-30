#include "SimplePicsJokeRequester.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>

#include "fnd/ScopedCall.h"

#include "network/network/downloader.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

struct SimplePicsJokeRequester::Impl
{
	const QString                                     fieldName;
	std::unordered_map<size_t, std::unique_ptr<Item>> requests;
	Network::Downloader                               downloader;
};

SimplePicsJokeRequester::SimplePicsJokeRequester(std::shared_ptr<Network::Downloader> downloader, QString uri, QString fieldName, QHttpHeaders headers)
	: BaseJokeRequester(std::move(downloader), std::move(uri), std::move(headers))
	, m_impl { std::move(fieldName) }
{
}

SimplePicsJokeRequester::~SimplePicsJokeRequester() = default;

bool SimplePicsJokeRequester::Process(const QJsonValue& value, std::weak_ptr<IClient> client)
{
	const auto jsonObject = [&]() -> QJsonObject {
		if (value.isObject())
			return value.toObject();

		if (!value.isArray())
			return {};

		const auto jsonArray = value.toArray();
		if (jsonArray.isEmpty())
			return {};

		const auto jsonArrayFront = *jsonArray.cbegin();
		if (!jsonArrayFront.isObject())
			return {};

		return jsonArrayFront.toObject();
	}();
	if (jsonObject.isEmpty())
		return false;

	const auto uri = jsonObject[m_impl->fieldName];
	if (!uri.isString())
		return false;

	auto       item = std::make_unique<Item>(std::move(client));
	const auto id   = m_impl->downloader.Download(uri.toString(), item->stream, [this](const size_t idMessage, const int code, const QString& message) {
        OnImageReceived(idMessage, code, message);
    });
	m_impl->requests.try_emplace(id, std::move(item));
	return true;
}

void SimplePicsJokeRequester::OnImageReceived(const size_t id, const int code, const QString& message)
{
	const auto it = m_impl->requests.find(id);
	assert(it != m_impl->requests.end());
	const ScopedCall requestGuard([&] {
		m_impl->requests.erase(it);
	});

	if (code != QNetworkReply::NetworkError::NoError)
		PLOGE << message;

	if (const auto client = it->second->client.lock())
		client->OnImageReceived(it->second->data);
}
