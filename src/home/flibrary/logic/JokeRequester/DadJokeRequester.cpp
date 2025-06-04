#include "DadJokeRequester.h"

#include <QHttpHeaders>

using namespace HomeCompa::Flibrary;

DadJokeRequester::DadJokeRequester()
	: SimpleJokeRequester("http://icanhazdadjoke.com/", "joke")
{
	QHttpHeaders headers;
	headers.append(QHttpHeaders::WellKnownHeader::Accept, "application/json");
	headers.append(QHttpHeaders::WellKnownHeader::UserAgent, "FLibrary (https://github.com/heimdallr/books)");
	SetHeaders(std::move(headers));
}
