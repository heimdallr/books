#include "CatFactJokeRequester.h"

using namespace HomeCompa::Flibrary;

CatFactJokeRequester::CatFactJokeRequester()
	: SimpleJokeRequester("http://catfact.ninja/fact", "fact")
{
}
