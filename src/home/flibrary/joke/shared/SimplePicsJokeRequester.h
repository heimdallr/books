#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{

class SimplePicsJokeRequester : public BaseJokeRequester
{
	NON_COPY_MOVABLE(SimplePicsJokeRequester)

public:
	SimplePicsJokeRequester(std::shared_ptr<Network::Downloader> downloader, QString uri, QString fieldName, QHttpHeaders headers = {});
	~SimplePicsJokeRequester() override;

private: // BaseJokeRequester
	bool Process(const QJsonValue& value, std::weak_ptr<IClient> client) override;

private:
	void OnImageReceived(size_t id, int code, const QString& message);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
