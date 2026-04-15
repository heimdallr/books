#include "QuotePicJokeRequester.h"

#include "interface/logic/IJokeRequesterFactory.h"

using namespace HomeCompa;
using namespace Flibrary;

QuotePicJokeRequester::QuotePicJokeRequester(const std::shared_ptr<const IJokeRequesterFactory>& jokeRequesterFactory)
	: BaseJokeRequester(jokeRequesterFactory->GetDownloader(), "https://zenquotes.io/api/image")
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
