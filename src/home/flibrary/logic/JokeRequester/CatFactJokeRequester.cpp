#include "CatFactJokeRequester.h"

#include "util/Localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto CONTEXT = "JokeRequester";
constexpr auto PREFIX = QT_TRANSLATE_NOOP("JokeRequester", "Interesting fact about cats");
TR_DEF
}

CatFactJokeRequester::CatFactJokeRequester()
	: SimpleJokeRequester("http://catfact.ninja/fact", "fact", Tr(PREFIX))
{
}
