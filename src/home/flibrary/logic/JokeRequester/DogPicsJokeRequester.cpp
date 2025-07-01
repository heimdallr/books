#include "DogPicsJokeRequester.h"

#include "interface/logic/IJokeRequesterFactory.h"

using namespace HomeCompa::Flibrary;

DogPicsJokeRequester::DogPicsJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory)
	: SimplePicsJokeRequester(jokeRequesterFactory->GetDownloader(), "http://dog.ceo/api/breeds/image/random", "message")
{
}
