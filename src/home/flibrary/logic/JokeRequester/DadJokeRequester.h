#pragma once

#include "shared/SimpleJokeRequester.h"

namespace HomeCompa::Flibrary
{

class IJokeRequesterFactory;

class DadJokeRequester final : public SimpleJokeRequester
{
public:
	explicit DadJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory);
};

}
