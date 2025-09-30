#pragma once

#include <map>
#include <string>

#include "fnd/observer.h"

class QJsonValue;

namespace HomeCompa::RestAPI
{

struct IConnection // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Headers = std::map<std::string, std::string, std::less<>>;

public:
	class IObserver : public Observer
	{
	public:
		virtual void HandleReceivedData(const QJsonValue& data) = 0;
	};

public:
	virtual ~IConnection() = default;

	virtual void               Get(const std::string& request) = 0;
	virtual const std::string& Url() const noexcept            = 0;
	virtual const Headers&     GetHeaders() const              = 0;

	virtual void Register(IObserver* observer)   = 0;
	virtual void Unregister(IObserver* observer) = 0;
};

} // namespace HomeCompa::RestAPI
