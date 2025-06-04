#pragma once

#include "shared/BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{

class QuotePicJokeRequester final : public BaseJokeRequester
{
public:
	QuotePicJokeRequester();

private:
	bool Process(const QByteArray& data, std::weak_ptr<IClient> client) override;
};

}
