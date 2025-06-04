#include "UselessFactJokeRequester.h"

#include <QHttpHeaders>

#include "util/Localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto CONTEXT = "JokeRequester";
constexpr auto PREFIX = QT_TRANSLATE_NOOP("JokeRequester", "One more useless fact");
TR_DEF
}

UselessFactJokeRequester::UselessFactJokeRequester()
	: SimpleJokeRequester("https://uselessfacts.jsph.pl/api/v2/facts/random", "text", Tr(PREFIX))
{
	QHttpHeaders headers;
	headers.append(QHttpHeaders::WellKnownHeader::Accept, "application/json");
	SetHeaders(std::move(headers));
}
