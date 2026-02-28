#pragma once

#include "shared/BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{

class IJokeRequesterFactory;

class QuotePicJokeRequester final : public BaseJokeRequester
{
public:
	explicit QuotePicJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory);

private:
	bool Process(const QByteArray& data, std::weak_ptr<IClient> client) override;
};

}
