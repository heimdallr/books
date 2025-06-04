#pragma once

#include "shared/BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{

class QuoteJokeRequester final : public BaseJokeRequester
{
public:
	QuoteJokeRequester();

private:
	bool Process(const QJsonValue& value, std::weak_ptr<IClient> client) override;
};

}
