#include "QuoteJokeRequester.h"

#include <QJsonArray>
#include <QJsonObject>

#include "util/Localization.h"

using namespace HomeCompa;
using namespace Flibrary;

QuoteJokeRequester::QuoteJokeRequester()
	: BaseJokeRequester("https://zenquotes.io/api/random")
{
}

bool QuoteJokeRequester::Process(const QJsonValue& value, std::weak_ptr<IClient> clientPtr)
{
	const auto client = clientPtr.lock();
	if (!client)
		return true;

	if (!value.isArray())
		return false;

	const auto jsonArray = value.toArray();
	if (jsonArray.isEmpty())
		return false;

	const auto jsonArrayFront = *value.toArray().cbegin();
	if (!jsonArrayFront.isObject())
		return false;

	const auto jsonObject = jsonArrayFront.toObject();
	const auto a = jsonObject["a"], q = jsonObject["q"];
	if (!a.isString() || !q.isString())
		return false;

	client->OnTextReceived(QString(R"(<p>%1</p><p style="font-style:italic;">%2</p>)").arg(q.toString(), a.toString()));
	return true;
}
