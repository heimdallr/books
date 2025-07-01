#pragma once

#include "shared/SimplePicsJokeRequester.h"

namespace HomeCompa::Flibrary
{
class IJokeRequesterFactory;

class FoxPicsJokeRequester final : public SimplePicsJokeRequester
{
public:
	explicit FoxPicsJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory);
};

}
