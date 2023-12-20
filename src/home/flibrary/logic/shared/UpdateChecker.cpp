#include "UpdateChecker.h"

#include "network/rest/api/github/Release.h"
#include "network/rest/api/github/Requester.h"
#include "network/rest/connection/ConnectionFactory.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa::RestAPI;
using namespace Github;

struct UpdateChecker::Impl final : virtual IClient
{
	Requester requester { CreateQtConnection("https://api.github.com") };
	Callback callback;

	void HandleLatestRelease(const Release & release) override
	{
		callback(release);
	}
};

UpdateChecker::UpdateChecker()
	: m_impl(std::make_shared<Impl>())
{
}

UpdateChecker::~UpdateChecker() = default;

void UpdateChecker::CheckForUpdate(Callback callback)
{
	m_impl->callback = std::move(callback);
	m_impl->requester.GetLatestRelease(m_impl, "heimdallr", "books");
}
