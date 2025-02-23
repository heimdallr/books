#include "Release.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "Requester.h"

using namespace HomeCompa::RestAPI::Github;

namespace
{

template <typename T>
T deserialize(const QJsonObject& data, const QString& key)
{
	return data[key].toVariant().value<T>();
}

}

Assets Asset::ParseAssets(const QJsonValue& data)
{
	assert(data.isArray());
	Assets assets;
	for (const auto dataValue : data.toArray())
	{
		const auto dataObj = dataValue.toObject();
		assets.emplace_back(
#define ITEM(NAME) deserialize<decltype(NAME)>(dataObj, #NAME)
			ITEM(id),
			ITEM(name),
			ITEM(size),
			ITEM(download_count),
			ITEM(browser_download_url)
#undef ITEM
		);
	}

	return assets;
}

void Release::ParseGetLatestRelease(IClient& client, const QJsonValue& data)
{
	const Release release(data);
	client.HandleLatestRelease(release);
}

Release::Release(const QJsonValue& data)
#define ITEM(NAME) NAME(deserialize<decltype(NAME)>(data.toObject(), #NAME))
	: ITEM(id)
	, ITEM(name)
	, ITEM(tag_name)
	, ITEM(html_url)
#undef ITEM
	, assets(Asset::ParseAssets(data["assets"]))
{
}
