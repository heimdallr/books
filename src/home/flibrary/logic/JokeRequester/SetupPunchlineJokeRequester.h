#pragma once

#include "shared/SimpleSetupPunchlineJokeRequester.h"

namespace HomeCompa::Flibrary
{
class IJokeRequesterFactory;

class SetupPunchlineJokeRequester final : public SimpleSetupPunchlineJokeRequester
{
public:
	explicit SetupPunchlineJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory);
};

}
