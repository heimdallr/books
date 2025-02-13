#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "connection/BaseConnection.h"

class QNetworkAccessManager;

namespace HomeCompa::RestAPI::QtLib
{

class Connection : public BaseConnection
{
	NON_COPY_MOVABLE(Connection)

public:
	Connection(std::string address, Headers headers);
	~Connection() override;

protected: // BaseConnection
	Headers GetPage(const std::string& request) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
