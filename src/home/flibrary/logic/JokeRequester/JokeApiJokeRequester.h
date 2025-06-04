#pragma once

#include "shared/BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{

class JokeApiJokeRequester final : public BaseJokeRequester
{
public:
	JokeApiJokeRequester();

private:
	bool Process(const QJsonValue& value, std::weak_ptr<IClient> client) override;
};

}
