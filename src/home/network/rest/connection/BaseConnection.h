#pragma once

#include <map>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "IConnection.h"

class QJsonDocument;

namespace HomeCompa::RestAPI
{

class BaseConnection : virtual public IConnection
{
	NON_COPY_MOVABLE(BaseConnection)

protected:
	BaseConnection(std::string address, Headers headers);
	~BaseConnection() override;

protected: // IConnection
	void               Get(const std::string& request) final;
	const std::string& Url() const noexcept final;
	const Headers&     GetHeaders() const noexcept final;
	void               Register(IObserver* observer) final;
	void               Unregister(IObserver* observer) final;

private:
	virtual Headers GetPage(const std::string& request) = 0;

protected:
	const std::string& GetAddress() const noexcept;
	void               OnDataReceived(const QJsonDocument& json);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::RestAPI
