#include "Requester.h"

#include <unordered_map>
#include <plog/Log.h>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "Release.h"

// Based on:
// https://developer.github.com/guides/getting-started/
// https://developer.github.com/v3/

using namespace HomeCompa::RestAPI::Github;

namespace {

enum class RequestType
{
	None,
	GetUserInfo,
	ListReleases,
	GetLatestRelease,
	GetRelease,
	GetRateLimit,
	ListUserRepo,
	GetAuthenticatedUser,
	ListUsers,
	GetUser,
	ListIssues,
	ListOrgIssues,
	ListRepoIssues,
	GetIssue,
	ListPullRequest,
	GetPullRequest,
	ListPullRequestCommit,
	ListPullRequestFiles,
	IsPullRequestMerged,
	ListCommits,
	ListBranchHeadCommit,
	ListCommitPullRequest,
	GetCommits,
	GetWeeklyCommit,
	GetLastYearCommit,
	GetContributorsActivity,
	GetCommitCount,
	GetHourlyCommitCount,
	GetCommunityProfileMetrics,
	GetRepoClones,
	GetReferralPaths,
	GetTopReferralSource,
	GetPageViews,
	ListNetworkRepoEvent,
	ListOrgEvent,
	ListRepoEvent,
	ListUserEvent,
	ListStargazers,
	ListUserStarredRepo,
	ListRepoWatchers,
	GetRepoSubscription,
	ListUserWatchedRepos,
	ListRepoCollaborators,
	GetOrgRepo,
	GetRepository,
	ListAuthUserRepo,
	GetRepoLang,
	RepoContributors,
};

}

class Requester::Impl : IConnection::IObserver
{
	NON_COPY_MOVABLE(Impl)

private:
	using ObjectHandler = void(*)(IClient & /*client*/, const QJsonValue & /*data*/);

public:
	explicit Impl(std::unique_ptr<IConnection> connection)
		: m_connection(std::move(connection))
	{
		m_connection->Register(this);
	}

	~Impl() override
	{
		m_connection->Unregister(this);
	}

	int64_t DoRequest(std::weak_ptr<IClient> client, const std::string & request, const RequestType type)
	{
		m_client = std::move(client);
		m_type = type;
		m_connection->Get(request);
		return 0;
	}

private: // IConnection::IObserver
	void HandleReceivedData(const QJsonValue & data) override
	{
		static constexpr std::pair<RequestType, ObjectHandler> handlers[]
		{
			{RequestType::GetLatestRelease, &Release::ParseGetLatestRelease},
		};

		if (const auto client = m_client.lock())
			std::invoke(FindSecond(handlers, m_type), *client, data);
	}

private:
	PropagateConstPtr<IConnection> m_connection;
	std::weak_ptr<IClient> m_client;
	RequestType m_type { RequestType::None };
};

Requester::Requester(std::unique_ptr<IConnection> connection)
	: m_impl(std::move(connection))
{
}

Requester::~Requester() = default;

int64_t Requester::GetUserInfo(std::weak_ptr<IClient> client, const std::string & user)
{
	const std::string request = std::string("users/") + user;
	return m_impl->DoRequest(std::move(client), request, RequestType::GetUserInfo);
}

int64_t Requester::ListUserRepo(std::weak_ptr<IClient> client, const std::string & user)
{
	const std::string request = std::string("users/") + user + "/repos";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListUserRepo);
}

int64_t Requester::ListReleases(std::weak_ptr<IClient> client, const std::string & user, const std::string & repo)
{
	const std::string request = std::string("repos/") + user + "/" + repo + "/releases";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListReleases);
}

int64_t Requester::GetLatestRelease(std::weak_ptr<IClient> client, const std::string & user, const std::string & repo)
{
	const std::string request = std::string("repos/") + user + "/" + repo + "/releases/latest";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetLatestRelease);
}

int64_t Requester::GetRelease(std::weak_ptr<IClient> client, const std::string & user, const std::string & repo, const int id)
{
	const std::string request = std::string("repos/") + user + "/" + repo + "/releases/" + std::to_string(id);
	return m_impl->DoRequest(std::move(client), request, RequestType::GetRelease);
}


int64_t Requester::GetRateLimit(std::weak_ptr<IClient> client)
{
	return m_impl->DoRequest(std::move(client), "rate_limit", RequestType::GetRateLimit);
}


int64_t Requester::GetAuthenticatedUser(std::weak_ptr<IClient> client)
{
	const std::string request = std::string("user");
	return m_impl->DoRequest(std::move(client), request, RequestType::GetAuthenticatedUser);
}

int64_t Requester::ListUsers(std::weak_ptr<IClient> client)
{
	const std::string request = std::string("users");
	return m_impl->DoRequest(std::move(client), request, RequestType::ListUsers);
}

int64_t Requester::GetUser(std::weak_ptr<IClient> client, const std::string & username)
{
	const std::string request = std::string("users/") + username;
	return m_impl->DoRequest(std::move(client), request, RequestType::GetUser);
}

int64_t Requester::ListIssues(std::weak_ptr<IClient> client)
{
	const std::string request = std::string("issues");
	return m_impl->DoRequest(std::move(client), request, RequestType::ListIssues);
}

int64_t Requester::ListOrgIssues(std::weak_ptr<IClient> client, const std::string & org)
{
	const std::string request = std::string("orgs/") + org + "/issues";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListOrgIssues);
}

int64_t Requester::ListRepoIssues(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/issues";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListRepoIssues);
}

int64_t Requester::GetIssue(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo, const std::string & issueNumber)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/issues/" + issueNumber;
	return m_impl->DoRequest(std::move(client), request, RequestType::GetIssue);
}

int64_t Requester::ListPullRequest(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/pulls";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListPullRequest);
}

int64_t Requester::GetPullRequest(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo, const std::string & pullNumber)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/pulls/" + pullNumber;
	return m_impl->DoRequest(std::move(client), request, RequestType::GetPullRequest);
}

int64_t Requester::ListPullRequestCommit(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo, const std::string & pullNumber)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/pulls/" + pullNumber + "/commits";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListPullRequestCommit);
}

int64_t Requester::ListPullRequestFiles(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo, const std::string & pullNumber)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/pulls/" + pullNumber + "/files";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListPullRequestFiles);
}

int64_t Requester::IsPullRequestMerged(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo, const std::string & pullNumber)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/pulls/" + pullNumber + "/merge";
	return m_impl->DoRequest(std::move(client), request, RequestType::IsPullRequestMerged);
}

int64_t Requester::ListCommits(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/issues";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListCommits);
}

int64_t Requester::ListBranchHeadCommit(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo, const std::string & commitSha)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/commits/" + commitSha + "/branches-where-head";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListBranchHeadCommit);
}

int64_t Requester::ListCommitPullRequest(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo, const std::string & commitSha)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/commits/" + commitSha + "/pulls";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListCommitPullRequest);
}

int64_t Requester::GetCommits(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo, const std::string & reference)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/commits/" + reference;
	return m_impl->DoRequest(std::move(client), request, RequestType::GetCommits);
}

int64_t Requester::GetWeeklyCommit(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/stats/code_frequency";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetWeeklyCommit);
}

int64_t Requester::GetLastYearCommit(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/stats/commit_activity";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetLastYearCommit);
}

int64_t Requester::GetContributorsActivity(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/stats/contributors";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetContributorsActivity);
}

int64_t Requester::GetCommitCount(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/stats/participation";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetCommitCount);
}

int64_t Requester::GetHourlyCommitCount(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/stats/punch_card";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetHourlyCommitCount);
}

int64_t Requester::GetCommunityProfileMetrics(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/community/profile";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetCommunityProfileMetrics);
}

int64_t Requester::GetRepoClones(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/traffic/clones";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetRepoClones);
}

int64_t Requester::GetReferralPaths(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/traffic/popular/paths";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetReferralPaths);
}

int64_t Requester::GetTopReferralSource(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/traffic/popular/referrers";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetTopReferralSource);
}

int64_t Requester::GetPageViews(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/traffic/views";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetPageViews);
}

int64_t Requester::ListNetworkRepoEvent(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("network/") + owner + "/" + repo + "/events";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListNetworkRepoEvent);
}

int64_t Requester::ListOrgEvent(std::weak_ptr<IClient> client, const std::string & org)
{
	const std::string request = std::string("orgs/") + org + "/" + "/events";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListOrgEvent);
}

int64_t Requester::ListRepoEvent(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/events";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListRepoEvent);
}

int64_t Requester::ListUserEvent(std::weak_ptr<IClient> client, const std::string & username)
{
	const std::string request = std::string("users/") + username + "/events";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListUserEvent);
}

int64_t Requester::ListStargazers(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/stargazers";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListStargazers);
}

int64_t Requester::ListUserStarredRepo(std::weak_ptr<IClient> client, const std::string & username)
{
	const std::string request = std::string("users/") + username + "/starred";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListUserStarredRepo);
}

int64_t Requester::ListRepoWatchers(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/subscribers";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListRepoWatchers);
}


int64_t Requester::GetRepoSubscription(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/subscription";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetRepoSubscription);
}

int64_t Requester::ListUserWatchedRepos(std::weak_ptr<IClient> client, const std::string & username)
{
	const std::string request = std::string("users/") + username + "/subscription";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListUserWatchedRepos);
}

int64_t Requester::ListRepoCollaborators(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/collaborators";
	return m_impl->DoRequest(std::move(client), request, RequestType::ListRepoCollaborators);
}

int64_t Requester::GetOrgRepo(std::weak_ptr<IClient> client, const std::string & org)
{
	const std::string request = std::string("orgs/") + org + "/repos";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetOrgRepo);
}

int64_t Requester::GetRepository(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo;
	return m_impl->DoRequest(std::move(client), request, RequestType::GetRepository);
}

int64_t Requester::ListAuthUserRepo(std::weak_ptr<IClient> client)
{
	const std::string request = std::string("user/repos");
	return m_impl->DoRequest(std::move(client), request, RequestType::ListAuthUserRepo);
}

int64_t Requester::GetRepoLang(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/languages";
	return m_impl->DoRequest(std::move(client), request, RequestType::GetRepoLang);
}

int64_t Requester::RepoContributors(std::weak_ptr<IClient> client, const std::string & owner, const std::string & repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/contributors";
	return m_impl->DoRequest(std::move(client), request, RequestType::RepoContributors);
}
