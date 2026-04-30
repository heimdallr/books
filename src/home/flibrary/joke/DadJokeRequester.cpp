#include "DadJokeRequester.h"

#include "interface/localization.h"
#include "interface/logic/IJokeRequesterFactory.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "JokeRequester";
constexpr auto PREFIX  = QT_TRANSLATE_NOOP("JokeRequester", "From dad");
TR_DEF

Network::Headers GetHeaders()
{
	Network::Headers headers;
	headers.Append("Accept", "application/json");
	headers.Append("UserAgent", "FLibrary (https://github.com/heimdallr/books)");
	return headers;
}

}

DadJokeRequester::DadJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory)
	: SimpleJokeRequester(jokeRequesterFactory->GetDownloader(), "https://icanhazdadjoke.com/", "joke", Tr(PREFIX), GetHeaders())
{
}
