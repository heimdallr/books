#pragma once

#include "BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{

class SetupPunchlineJokeRequester final : public BaseJokeRequester
{
public:
	SetupPunchlineJokeRequester();

private: // BaseJokeRequester
	bool Process(const QJsonValue& value, const std::weak_ptr<IClient>& client) override;
};

}
