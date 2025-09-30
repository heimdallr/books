#include "CatFactJokeRequester.h"

#include "interface/logic/IJokeRequesterFactory.h"

#include "util/Localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto CONTEXT = "JokeRequester";
constexpr auto PREFIX  = QT_TRANSLATE_NOOP("JokeRequester", "Interesting fact about cats");
TR_DEF
}

CatFactJokeRequester::CatFactJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory)
	: SimpleJokeRequester(jokeRequesterFactory->GetDownloader(), "http://catfact.ninja/fact", "fact", Tr(PREFIX))
{
}
