#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/IServer.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Opds {

class Server : virtual public IServer
{
	NON_COPY_MOVABLE(Server)

public:
	Server(const std::shared_ptr<const ISettings> & settings
	);
	~Server() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
