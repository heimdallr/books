#include "ConnectionFactory.h"

#include "connection/qt/Connection.h"

namespace HomeCompa::RestAPI
{

std::unique_ptr<IConnection> CreateQtConnection(std::string address, IConnection::Headers headers)
{
	return std::make_unique<QtLib::Connection>(std::move(address), std::move(headers));
}

}
