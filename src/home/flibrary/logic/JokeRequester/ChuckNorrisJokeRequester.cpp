#include "ChuckNorrisJokeRequester.h"

#include "interface/logic/IJokeRequesterFactory.h"

#include "util/Localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto CONTEXT = "JokeRequester";
constexpr auto PREFIX = QT_TRANSLATE_NOOP("JokeRequester", "Important fact about Chuck Norris");
TR_DEF
}

ChuckNorrisJokeRequester::ChuckNorrisJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory)
	: SimpleJokeRequester(jokeRequesterFactory->GetDownloader(), "http://api.chucknorris.io/jokes/random", "value", Tr(PREFIX))
{
}
