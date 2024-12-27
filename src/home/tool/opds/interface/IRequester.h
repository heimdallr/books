#pragma once

class QByteArray;

namespace HomeCompa::Opds {

class IRequester  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IRequester() = default;

	virtual QByteArray GetRoot() const = 0;
};

}
