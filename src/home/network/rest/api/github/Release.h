#pragma once

#include <QString>

class QJsonValue;

namespace HomeCompa::RestAPI::Github
{

class IClient;

struct Asset;
using Assets = std::vector<Asset>;

struct Asset
{
	int64_t id { -1 };
	QString name;
	int64_t size { -1 };
	int64_t download_count { -1 };
	QString browser_download_url;

	static Assets ParseAssets(const QJsonValue& data);
};

struct Release
{
	int64_t id { -1 };
	QString name;
	QString tag_name;
	QString html_url;
	Assets  assets;

	static void ParseGetLatestRelease(IClient& client, const QJsonValue& data);

	explicit Release(const QJsonValue& data);
	Release()                          = default;
	~Release()                         = default;
	Release(const Release&)            = default;
	Release(Release&&)                 = default;
	Release& operator=(const Release&) = default;
	Release& operator=(Release&&)      = default;
};

} // namespace HomeCompa::RestAPI::Github
