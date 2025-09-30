#include "UselessFactJokeRequester.h"

#include <QHttpHeaders>

#include "interface/logic/IJokeRequesterFactory.h"

#include "util/Localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto CONTEXT = "JokeRequester";
constexpr auto PREFIX  = QT_TRANSLATE_NOOP("JokeRequester", "One more useless fact");
TR_DEF

QHttpHeaders CreateHeaders()
{
	QHttpHeaders headers;
	headers.append(QHttpHeaders::WellKnownHeader::Accept, "application/json");
	return headers;
}

}

UselessFactJokeRequester::UselessFactJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory)
	: SimpleJokeRequester(jokeRequesterFactory->GetDownloader(), "https://uselessfacts.jsph.pl/api/v2/facts/random", "text", Tr(PREFIX), CreateHeaders())
{
}
