#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/IServer.h"

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::Opds
{

class Server : virtual public IServer
{
	NON_COPY_MOVABLE(Server)

public:
	Server(const std::shared_ptr<const ISettings>& settings, std::shared_ptr<class IRequester> requester);
	~Server() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
