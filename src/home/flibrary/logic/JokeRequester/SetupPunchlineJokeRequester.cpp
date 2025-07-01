#include "SetupPunchlineJokeRequester.h"

#include "interface/logic/IJokeRequesterFactory.h"

using namespace HomeCompa::Flibrary;

SetupPunchlineJokeRequester::SetupPunchlineJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory)
	: SimpleSetupPunchlineJokeRequester(jokeRequesterFactory->GetDownloader(), "https://official-joke-api.appspot.com/random_joke", "setup", "punchline")
{
}
