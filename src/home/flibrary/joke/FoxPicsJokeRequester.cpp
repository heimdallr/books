#include "FoxPicsJokeRequester.h"

#include "interface/logic/IJokeRequesterFactory.h"

using namespace HomeCompa::Flibrary;

FoxPicsJokeRequester::FoxPicsJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory)
	: SimplePicsJokeRequester(jokeRequesterFactory->GetDownloader(), "https://randomfox.ca/floof/", "image")
{
}
