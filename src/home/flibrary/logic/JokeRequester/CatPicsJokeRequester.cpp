#include "CatPicsJokeRequester.h"

#include "interface/logic/IJokeRequesterFactory.h"

using namespace HomeCompa::Flibrary;

CatPicsJokeRequester::CatPicsJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory)
	: SimplePicsJokeRequester(jokeRequesterFactory->GetDownloader(), "http://api.thecatapi.com/v1/images/search", "url")
{
}
