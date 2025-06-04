#include "SetupPunchlineJokeRequester.h"

#include <QJsonObject>

using namespace HomeCompa::Flibrary;

SetupPunchlineJokeRequester::SetupPunchlineJokeRequester()
	: SimpleSetupPunchlineJokeRequester("https://official-joke-api.appspot.com/random_joke", "setup", "punchline")
{
}
