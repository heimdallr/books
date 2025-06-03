#include "DogPicsJokeRequester.h"

using namespace HomeCompa::Flibrary;

DogPicsJokeRequester::DogPicsJokeRequester()
	: SimplePicsJokeRequester("http://dog.ceo/api/breeds/image/random", "message")
{
}
