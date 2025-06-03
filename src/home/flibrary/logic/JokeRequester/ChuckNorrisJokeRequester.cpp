#include <QJsonObject>
#include <QString>

#include "ChuckNorrisJokeRequester.h"

using namespace HomeCompa::Flibrary;

ChuckNorrisJokeRequester::ChuckNorrisJokeRequester()
	: BaseJokeRequester("http://api.chucknorris.io/jokes/random")
{
}

ChuckNorrisJokeRequester::~ChuckNorrisJokeRequester() = default;

bool ChuckNorrisJokeRequester::Process(const QJsonValue& value, const std::weak_ptr<IClient>& clientPtr)
{
	const auto client = clientPtr.lock();
	if (!client)
		return true;

	if (!value.isObject())
		return false;

	const auto jsonObject = value.toObject();
	const auto text = jsonObject["value"];
	if (!text.isString())
		return false;

	client->OnTextReceived(text.toString());
	return true;
}
