#pragma once

#include "shared/SimpleJokeRequester.h"

namespace HomeCompa::Flibrary
{

class IJokeRequesterFactory;

class CatFactJokeRequester final : public SimpleJokeRequester
{
public:
	explicit CatFactJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory);
};

}
