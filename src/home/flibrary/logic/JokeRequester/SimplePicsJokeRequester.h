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
	SimplePicsJokeRequester(QString uri, QString fieldName);
	~SimplePicsJokeRequester() override;

private: // BaseJokeRequester
	bool Process(const QJsonValue& value, const std::weak_ptr<IClient>& client) override;

private:
	void OnImageReceived(size_t id, int code, const QString& message);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
