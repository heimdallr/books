#pragma once

#include <memory>

#include "IConnection.h"

#include "export/rest.h"

namespace HomeCompa::RestAPI
{

REST_EXPORT std::unique_ptr<IConnection> CreateQtConnection(std::string address, IConnection::Headers headers = {});

}
