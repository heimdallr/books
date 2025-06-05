#pragma once

#include "shared/SimpleJokeRequester.h"

namespace HomeCompa::Flibrary
{
class IJokeRequesterFactory;

class UselessFactJokeRequester final : public SimpleJokeRequester
{
public:
	explicit UselessFactJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory);
};

}
