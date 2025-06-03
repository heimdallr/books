#include "ChuckNorrisJokeRequester.h"

using namespace HomeCompa::Flibrary;

ChuckNorrisJokeRequester::ChuckNorrisJokeRequester()
	: SimpleJokeRequester("http://api.chucknorris.io/jokes/random", "value")
{
}
