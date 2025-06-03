#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "BaseJokeRequester.h"

namespace HomeCompa::Flibrary
{

class DogPicsJokeRequester final : public BaseJokeRequester
{
	NON_COPY_MOVABLE(DogPicsJokeRequester)

public:
	DogPicsJokeRequester();
	~DogPicsJokeRequester() override;

private: // BaseJokeRequester
	bool Process(const QJsonValue& value, const std::weak_ptr<IClient>& client) override;

private:
	void OnImageReceived(size_t id, int code, const QString& message);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
