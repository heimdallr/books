#include "DadJokeRequester.h"

#include <QHttpHeaders>

#include "interface/logic/IJokeRequesterFactory.h"

#include "util/Localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto CONTEXT = "JokeRequester";
constexpr auto PREFIX = QT_TRANSLATE_NOOP("JokeRequester", "From dad");
TR_DEF

QHttpHeaders GetHeaders()
{
	QHttpHeaders headers;
	headers.append(QHttpHeaders::WellKnownHeader::Accept, "application/json");
	headers.append(QHttpHeaders::WellKnownHeader::UserAgent, "FLibrary (https://github.com/heimdallr/books)");
	return headers;
}

}

DadJokeRequester::DadJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory)
	: SimpleJokeRequester(jokeRequesterFactory->GetDownloader(), "http://icanhazdadjoke.com/", "joke", Tr(PREFIX), GetHeaders())
{
}
