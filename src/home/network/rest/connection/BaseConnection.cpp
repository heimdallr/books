#include "BaseConnection.h"

#include <regex>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <plog/Log.h>

#include "fnd/observable.h"

using namespace HomeCompa::RestAPI;

namespace {

std::string GetNextPage(const IConnection::Headers & headers)
{
	const auto it = headers.find("link");
	if (it == headers.end())
		return {};

	const std::regex nextPageRegex(R"(^.*<([^>]+)>; rel="next".*)");
	std::smatch nextPageMatch;
	if (std::regex_match(it->second, nextPageMatch, nextPageRegex))
		return nextPageMatch[1];

	return {};
}

}

struct BaseConnection::Impl final : Observable<IObserver>
{
	const std::string address;
	Headers headers;
	std::vector<QJsonValue> data;

	Impl(std::string address, Headers headers)
		: address(std::move(address))
		, headers(std::move(headers))
	{
	}
};

BaseConnection::BaseConnection(std::string address, Headers headers)
	: m_impl(std::move(address), std::move(headers))
{
}

BaseConnection::~BaseConnection() = default;

void BaseConnection::Get(const std::string & request)
{
	PLOGD << "Request: " << request;
	for (auto page = m_impl->address + "/" + request; !page.empty(); )
	{
		const auto headers = GetPage(page);
		page = GetNextPage(headers);
	}

	assert(m_impl->data.size() == 1 && "@todo unite data");
	m_impl->Perform(&IObserver::HandleReceivedData, m_impl->data.front());
}


const std::string & BaseConnection::Url() const noexcept
{
	return m_impl->address;
}

const IConnection::Headers & BaseConnection::GetHeaders() const noexcept
{
	return m_impl->headers;
}

void BaseConnection::Register(IObserver * observer)
{
	m_impl->Register(observer);
}

void BaseConnection::Unregister(IObserver * observer)
{
	m_impl->Unregister(observer);
}

const std::string & BaseConnection::GetAddress() const noexcept
{
	return m_impl->address;
}

void BaseConnection::OnDataReceived(const QJsonDocument & json)
{
	if (json.isArray())
		m_impl->data.emplace_back(json.array());
	else if (json.isObject())
		m_impl->data.emplace_back(json.object());
	else
		assert(false && "unexpected json type");
}
