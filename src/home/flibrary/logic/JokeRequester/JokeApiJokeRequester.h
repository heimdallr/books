#pragma once

#include "shared/BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{
class IJokeRequesterFactory;

class JokeApiJokeRequester final : public BaseJokeRequester
{
public:
	explicit JokeApiJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory);

private:
	bool Process(const QJsonValue& value, std::weak_ptr<IClient> client) override;
};

}
