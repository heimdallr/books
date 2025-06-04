#include "UselessFactJokeRequester.h"

#include <QHttpHeaders>

using namespace HomeCompa::Flibrary;

UselessFactJokeRequester::UselessFactJokeRequester()
	: SimpleJokeRequester("https://uselessfacts.jsph.pl/api/v2/facts/random", "text")
{
	QHttpHeaders headers;
	headers.append(QHttpHeaders::WellKnownHeader::Accept, "application/json");
	SetHeaders(std::move(headers));
}
