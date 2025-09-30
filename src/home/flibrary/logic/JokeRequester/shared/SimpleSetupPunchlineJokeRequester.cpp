#include "SimpleSetupPunchlineJokeRequester.h"

#include <QJsonObject>

using namespace HomeCompa::Flibrary;

SimpleSetupPunchlineJokeRequester::SimpleSetupPunchlineJokeRequester(std::shared_ptr<Network::Downloader> downloader, QString uri, QString setupField, QString punchlineField, QHttpHeaders headers)
	: BaseJokeRequester(std::move(downloader), std::move(uri), std::move(headers))
	, m_setupField { std::move(setupField) }
	, m_punchlineField { std::move(punchlineField) }
{
}

bool SimpleSetupPunchlineJokeRequester::Process(const QJsonValue& value, std::weak_ptr<IClient> clientPtr)
{
	const auto client = clientPtr.lock();
	if (!client)
		return true;

	if (!value.isObject())
		return false;

	const auto jsonObject = value.toObject();
	const auto setup      = jsonObject[m_setupField];
	const auto punchline  = jsonObject[m_punchlineField];
	if (!setup.isString() || !punchline.isString())
		return false;

	client->OnTextReceived(QString("<p>%1 %2</p><p>%1 %3</p>").arg(QChar(0x2014), setup.toString(), punchline.toString()));
	return true;
}
