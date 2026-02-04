#pragma once

#include "shared/SimplePicsJokeRequester.h"

namespace HomeCompa::Flibrary
{

class IJokeRequesterFactory;

class CatPicsJokeRequester final : public SimplePicsJokeRequester
{
public:
	explicit CatPicsJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory);
};

}
