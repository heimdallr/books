#include "SetupPunchlineJokeRequester.h"

#include <QJsonObject>

using namespace HomeCompa::Flibrary;

SetupPunchlineJokeRequester::SetupPunchlineJokeRequester()
	: BaseJokeRequester("https://official-joke-api.appspot.com/random_joke")
{
}

bool SetupPunchlineJokeRequester::Process(const QJsonValue& value, std::weak_ptr<IClient> clientPtr)
{
	const auto client = clientPtr.lock();
	if (!client)
		return true;

	if (!value.isObject())
		return false;

	const auto jsonObject = value.toObject();
	const auto setup = jsonObject["setup"];
	const auto punchline = jsonObject["punchline"];
	if (!setup.isString() || !punchline.isString())
		return false;

	client->OnTextReceived(QString("<p>%1 %2</p><p>%1 %3</p>").arg(QChar(0x2014), setup.toString(), punchline.toString()));
	return true;
}
