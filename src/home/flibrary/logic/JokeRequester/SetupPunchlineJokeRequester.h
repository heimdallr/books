#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IJokeRequester.h"

namespace HomeCompa::Flibrary
{

class SetupPunchlineJokeRequester : virtual public IJokeRequester
{
	NON_COPY_MOVABLE(SetupPunchlineJokeRequester)

public:
	SetupPunchlineJokeRequester();
	~SetupPunchlineJokeRequester() override;

private:
	void Request(std::weak_ptr<IClient> client) override;

private:
	void OnResponse(size_t id, int code, const QString& message);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
