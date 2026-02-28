#pragma once

#include "shared/SimpleJokeRequester.h"

namespace HomeCompa::Flibrary
{

class IJokeRequesterFactory;

class ChuckNorrisJokeRequester final : public SimpleJokeRequester
{
public:
	explicit ChuckNorrisJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory);
};

}
