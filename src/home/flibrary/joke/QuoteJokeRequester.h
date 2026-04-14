#pragma once

#include "shared/BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{

class IJokeRequesterFactory;

class QuoteJokeRequester final : public BaseJokeRequester
{
public:
	explicit QuoteJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory);

private:
	bool Process(const QJsonValue& value, std::weak_ptr<IClient> client) override;
};

}
