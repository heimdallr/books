#include "SimpleJokeRequester.h"

#include <QJsonObject>
#include <QString>

using namespace HomeCompa::Flibrary;

SimpleJokeRequester::SimpleJokeRequester(QString uri, QString fieldName, QString prefix)
	: BaseJokeRequester(std::move(uri))
	, m_fieldName { std::move(fieldName) }
	, m_prefix { std::move(prefix) }
{
}

SimpleJokeRequester::~SimpleJokeRequester() = default;

bool SimpleJokeRequester::Process(const QJsonValue& value, std::weak_ptr<IClient> clientPtr)
{
	const auto client = clientPtr.lock();
	if (!client)
		return true;

	if (!value.isObject())
		return false;

	const auto jsonObject = value.toObject();
	auto textValue = jsonObject[m_fieldName];
	if (!textValue.isString())
		return false;

	const auto text = textValue.toString();
	client->OnTextReceived(m_prefix.isEmpty() ? text : QString("<p><i>%1</i></p>%2").arg(m_prefix, text));

	return true;
}
