#include "DadJokeRequester.h"

#include <QHttpHeaders>

#include "util/Localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto CONTEXT = "JokeRequester";
constexpr auto PREFIX = QT_TRANSLATE_NOOP("JokeRequester", "From dad");
TR_DEF
}

DadJokeRequester::DadJokeRequester()
	: SimpleJokeRequester("http://icanhazdadjoke.com/", "joke", Tr(PREFIX))
{
	QHttpHeaders headers;
	headers.append(QHttpHeaders::WellKnownHeader::Accept, "application/json");
	headers.append(QHttpHeaders::WellKnownHeader::UserAgent, "FLibrary (https://github.com/heimdallr/books)");
	SetHeaders(std::move(headers));
}
