#include "QuotePicJokeRequester.h"

#include "util/Localization.h"

using namespace HomeCompa;
using namespace Flibrary;

QuotePicJokeRequester::QuotePicJokeRequester()
	: BaseJokeRequester("https://zenquotes.io/api/image")
{
}

bool QuotePicJokeRequester::Process(const QByteArray& data, std::weak_ptr<IClient> clientPtr)
{
	const auto client = clientPtr.lock();
	if (!client)
		return true;

	client->OnImageReceived(data);
	return true;
}
