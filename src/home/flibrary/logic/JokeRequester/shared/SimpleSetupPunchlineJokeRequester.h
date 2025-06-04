#pragma once

#include "BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{

class SimpleSetupPunchlineJokeRequester : public BaseJokeRequester
{
public:
	SimpleSetupPunchlineJokeRequester(QString uri, QString setupField, QString punchlineField);

private: // BaseJokeRequester
	bool Process(const QJsonValue& value, std::weak_ptr<IClient> client) override;

private:
	const QString m_setupField, m_punchlineField;
};

}
