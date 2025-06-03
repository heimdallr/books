#pragma once

#include "fnd/NonCopyMovable.h"

#include "BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{

class SetupPunchlineJokeRequester final : public BaseJokeRequester
{
	NON_COPY_MOVABLE(SetupPunchlineJokeRequester)

public:
	SetupPunchlineJokeRequester();
	~SetupPunchlineJokeRequester() override;

private: // BaseJokeRequester
	bool Process(const QJsonValue& value, const std::weak_ptr<IClient>& client) override;
};

}
