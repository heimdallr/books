#pragma once

#include "fnd/NonCopyMovable.h"

#include "BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{

class SimpleJokeRequester : public BaseJokeRequester
{
	NON_COPY_MOVABLE(SimpleJokeRequester)

public:
	SimpleJokeRequester(QString uri, QString fieldName, QString prefix = {});
	~SimpleJokeRequester() override;

private: // BaseJokeRequester
	bool Process(const QJsonValue& value, std::weak_ptr<IClient> client) override;

private:
	const QString m_fieldName;
	const QString m_prefix;
};

}
