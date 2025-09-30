#pragma once

#include <QBuffer>
#include <QHttpHeaders>
#include <QString>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IJokeRequester.h"

namespace HomeCompa::Network
{
class Downloader;
}

class QHttpHeaders;
class QJsonValue;

namespace HomeCompa::Flibrary
{

class BaseJokeRequester : virtual public IJokeRequester
{
	NON_COPY_MOVABLE(BaseJokeRequester)

protected:
	struct Item
	{
		std::weak_ptr<IClient> client;
		QByteArray             data;
		QBuffer                stream { &data };

		explicit Item(std::weak_ptr<IClient> client);
	};

public:
	BaseJokeRequester(std::shared_ptr<Network::Downloader> downloader, QString uri, QHttpHeaders headers = {});
	~BaseJokeRequester() override;

private: // IJokeRequester
	void Request(std::weak_ptr<IClient> client) override;

private:
	void         OnResponse(size_t id, int code, const QString& message);
	virtual bool Process(const QJsonValue& value, std::weak_ptr<IClient> client);
	virtual bool Process(const QByteArray& data, std::weak_ptr<IClient> client);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
