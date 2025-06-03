#include <QJsonObject>
#include <QString>

#include "SimpleJokeRequester.h"

using namespace HomeCompa::Flibrary;

SimpleJokeRequester::SimpleJokeRequester(QString uri, QString fieldName)
	: BaseJokeRequester(std::move(uri))
	, m_fieldName { std::move(fieldName) }
{
}

SimpleJokeRequester::~SimpleJokeRequester() = default;

bool SimpleJokeRequester::Process(const QJsonValue& value, const std::weak_ptr<IClient>& clientPtr)
{
	const auto client = clientPtr.lock();
	if (!client)
		return true;

	if (!value.isObject())
		return false;

	const auto jsonObject = value.toObject();
	const auto text = jsonObject[m_fieldName];
	if (!text.isString())
		return false;

	client->OnTextReceived(text.toString());
	return true;
}
