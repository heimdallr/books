#pragma once

#include "shared/SimplePicsJokeRequester.h"

namespace HomeCompa::Flibrary
{
class IJokeRequesterFactory;

class DogPicsJokeRequester final : public SimplePicsJokeRequester
{
public:
	explicit DogPicsJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory);
};

}
