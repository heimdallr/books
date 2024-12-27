#pragma once

namespace HomeCompa::Opds {

class IFactory  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IFactory() = default;

public:
	virtual std::shared_ptr<class IServer> CreateServer() const = 0;
};

}
