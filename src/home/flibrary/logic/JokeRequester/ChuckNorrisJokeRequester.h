#pragma once

#include "fnd/NonCopyMovable.h"

#include "BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{

class ChuckNorrisJokeRequester final : public BaseJokeRequester
{
	NON_COPY_MOVABLE(ChuckNorrisJokeRequester)

public:
	ChuckNorrisJokeRequester();
	~ChuckNorrisJokeRequester() override;

private: // BaseJokeRequester
	bool Process(const QJsonValue& value, const std::weak_ptr<IClient>& client) override;
};

}
