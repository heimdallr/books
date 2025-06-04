#include "CatPicsJokeRequester.h"

using namespace HomeCompa::Flibrary;

CatPicsJokeRequester::CatPicsJokeRequester()
	: SimplePicsJokeRequester("http://api.thecatapi.com/v1/images/search", "url")
{
}
