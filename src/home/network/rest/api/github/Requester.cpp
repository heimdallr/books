#include "Requester.h"

#include <unordered_map>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "Release.h"
#include "log.h"

// Based on:
// https://developer.github.com/guides/getting-started/
// https://developer.github.com/v3/

using namespace HomeCompa::RestAPI::Github;

namespace
{

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
	using ObjectHandler = void (*)(IClient& /*client*/, const QJsonValue& /*data*/);

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

	void DoRequest(std::weak_ptr<IClient> client, const std::string& request, const RequestType type)
	{
		m_client = std::move(client);
		m_type   = type;
		m_connection->Get(request);
	}

private: // IConnection::IObserver
	void HandleReceivedData(const QJsonValue& data) override
	{
		static constexpr std::pair<RequestType, ObjectHandler> handlers[] {
			{ RequestType::GetLatestRelease, &Release::ParseGetLatestRelease },
		};

		if (const auto client = m_client.lock())
			std::invoke(FindSecond(handlers, m_type), *client, data);
	}

private:
	PropagateConstPtr<IConnection> m_connection;
	std::weak_ptr<IClient>         m_client;
	RequestType                    m_type { RequestType::None };
};

Requester::Requester(std::unique_ptr<IConnection> connection)
	: m_impl(std::move(connection))
{
}

Requester::~Requester() = default;

void Requester::GetUserInfo(std::weak_ptr<IClient> client, const std::string& user)
{
	const std::string request = std::string("users/") + user;
	m_impl->DoRequest(std::move(client), request, RequestType::GetUserInfo);
}

void Requester::ListUserRepo(std::weak_ptr<IClient> client, const std::string& user)
{
	const std::string request = std::string("users/") + user + "/repos";
	m_impl->DoRequest(std::move(client), request, RequestType::ListUserRepo);
}

void Requester::ListReleases(std::weak_ptr<IClient> client, const std::string& user, const std::string& repo)
{
	const std::string request = std::string("repos/") + user + "/" + repo + "/releases";
	m_impl->DoRequest(std::move(client), request, RequestType::ListReleases);
}

void Requester::GetLatestRelease(std::weak_ptr<IClient> client, const std::string& user, const std::string& repo)
{
	const std::string request = std::string("repos/") + user + "/" + repo + "/releases/latest";
	m_impl->DoRequest(std::move(client), request, RequestType::GetLatestRelease);
}

void Requester::GetRelease(std::weak_ptr<IClient> client, const std::string& user, const std::string& repo, const int id)
{
	const std::string request = std::string("repos/") + user + "/" + repo + "/releases/" + std::to_string(id);
	m_impl->DoRequest(std::move(client), request, RequestType::GetRelease);
}

void Requester::GetRateLimit(std::weak_ptr<IClient> client)
{
	m_impl->DoRequest(std::move(client), "rate_limit", RequestType::GetRateLimit);
}

void Requester::GetAuthenticatedUser(std::weak_ptr<IClient> client)
{
	const std::string request = std::string("user");
	m_impl->DoRequest(std::move(client), request, RequestType::GetAuthenticatedUser);
}

void Requester::ListUsers(std::weak_ptr<IClient> client)
{
	const std::string request = std::string("users");
	m_impl->DoRequest(std::move(client), request, RequestType::ListUsers);
}

void Requester::GetUser(std::weak_ptr<IClient> client, const std::string& username)
{
	const std::string request = std::string("users/") + username;
	m_impl->DoRequest(std::move(client), request, RequestType::GetUser);
}

void Requester::ListIssues(std::weak_ptr<IClient> client)
{
	const std::string request = std::string("issues");
	m_impl->DoRequest(std::move(client), request, RequestType::ListIssues);
}

void Requester::ListOrgIssues(std::weak_ptr<IClient> client, const std::string& org)
{
	const std::string request = std::string("orgs/") + org + "/issues";
	m_impl->DoRequest(std::move(client), request, RequestType::ListOrgIssues);
}

void Requester::ListRepoIssues(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/issues";
	m_impl->DoRequest(std::move(client), request, RequestType::ListRepoIssues);
}

void Requester::GetIssue(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& issueNumber)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/issues/" + issueNumber;
	m_impl->DoRequest(std::move(client), request, RequestType::GetIssue);
}

void Requester::ListPullRequest(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/pulls";
	m_impl->DoRequest(std::move(client), request, RequestType::ListPullRequest);
}

void Requester::GetPullRequest(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& pullNumber)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/pulls/" + pullNumber;
	m_impl->DoRequest(std::move(client), request, RequestType::GetPullRequest);
}

void Requester::ListPullRequestCommit(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& pullNumber)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/pulls/" + pullNumber + "/commits";
	m_impl->DoRequest(std::move(client), request, RequestType::ListPullRequestCommit);
}

void Requester::ListPullRequestFiles(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& pullNumber)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/pulls/" + pullNumber + "/files";
	m_impl->DoRequest(std::move(client), request, RequestType::ListPullRequestFiles);
}

void Requester::IsPullRequestMerged(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& pullNumber)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/pulls/" + pullNumber + "/merge";
	m_impl->DoRequest(std::move(client), request, RequestType::IsPullRequestMerged);
}

void Requester::ListCommits(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/commits";
	m_impl->DoRequest(std::move(client), request, RequestType::ListCommits);
}

void Requester::ListBranchHeadCommit(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& commitSha)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/commits/" + commitSha + "/branches-where-head";
	m_impl->DoRequest(std::move(client), request, RequestType::ListBranchHeadCommit);
}

void Requester::ListCommitPullRequest(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& commitSha)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/commits/" + commitSha + "/pulls";
	m_impl->DoRequest(std::move(client), request, RequestType::ListCommitPullRequest);
}

void Requester::GetCommits(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& reference)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/commits/" + reference;
	m_impl->DoRequest(std::move(client), request, RequestType::GetCommits);
}

void Requester::GetWeeklyCommit(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/stats/code_frequency";
	m_impl->DoRequest(std::move(client), request, RequestType::GetWeeklyCommit);
}

void Requester::GetLastYearCommit(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/stats/commit_activity";
	m_impl->DoRequest(std::move(client), request, RequestType::GetLastYearCommit);
}

void Requester::GetContributorsActivity(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/stats/contributors";
	m_impl->DoRequest(std::move(client), request, RequestType::GetContributorsActivity);
}

void Requester::GetCommitCount(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/stats/participation";
	m_impl->DoRequest(std::move(client), request, RequestType::GetCommitCount);
}

void Requester::GetHourlyCommitCount(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/stats/punch_card";
	m_impl->DoRequest(std::move(client), request, RequestType::GetHourlyCommitCount);
}

void Requester::GetCommunityProfileMetrics(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/community/profile";
	m_impl->DoRequest(std::move(client), request, RequestType::GetCommunityProfileMetrics);
}

void Requester::GetRepoClones(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/traffic/clones";
	m_impl->DoRequest(std::move(client), request, RequestType::GetRepoClones);
}

void Requester::GetReferralPaths(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/traffic/popular/paths";
	m_impl->DoRequest(std::move(client), request, RequestType::GetReferralPaths);
}

void Requester::GetTopReferralSource(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/traffic/popular/referrers";
	m_impl->DoRequest(std::move(client), request, RequestType::GetTopReferralSource);
}

void Requester::GetPageViews(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/traffic/views";
	m_impl->DoRequest(std::move(client), request, RequestType::GetPageViews);
}

void Requester::ListNetworkRepoEvent(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("network/") + owner + "/" + repo + "/events";
	m_impl->DoRequest(std::move(client), request, RequestType::ListNetworkRepoEvent);
}

void Requester::ListOrgEvent(std::weak_ptr<IClient> client, const std::string& org)
{
	const std::string request = std::string("orgs/") + org + "/" + "/events";
	m_impl->DoRequest(std::move(client), request, RequestType::ListOrgEvent);
}

void Requester::ListRepoEvent(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/events";
	m_impl->DoRequest(std::move(client), request, RequestType::ListRepoEvent);
}

void Requester::ListUserEvent(std::weak_ptr<IClient> client, const std::string& username)
{
	const std::string request = std::string("users/") + username + "/events";
	m_impl->DoRequest(std::move(client), request, RequestType::ListUserEvent);
}

void Requester::ListStargazers(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/stargazers";
	m_impl->DoRequest(std::move(client), request, RequestType::ListStargazers);
}

void Requester::ListUserStarredRepo(std::weak_ptr<IClient> client, const std::string& username)
{
	const std::string request = std::string("users/") + username + "/starred";
	m_impl->DoRequest(std::move(client), request, RequestType::ListUserStarredRepo);
}

void Requester::ListRepoWatchers(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/subscribers";
	m_impl->DoRequest(std::move(client), request, RequestType::ListRepoWatchers);
}

void Requester::GetRepoSubscription(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/subscription";
	m_impl->DoRequest(std::move(client), request, RequestType::GetRepoSubscription);
}

void Requester::ListUserWatchedRepos(std::weak_ptr<IClient> client, const std::string& username)
{
	const std::string request = std::string("users/") + username + "/subscription";
	m_impl->DoRequest(std::move(client), request, RequestType::ListUserWatchedRepos);
}

void Requester::ListRepoCollaborators(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/collaborators";
	m_impl->DoRequest(std::move(client), request, RequestType::ListRepoCollaborators);
}

void Requester::GetOrgRepo(std::weak_ptr<IClient> client, const std::string& org)
{
	const std::string request = std::string("orgs/") + org + "/repos";
	m_impl->DoRequest(std::move(client), request, RequestType::GetOrgRepo);
}

void Requester::GetRepository(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo;
	m_impl->DoRequest(std::move(client), request, RequestType::GetRepository);
}

void Requester::ListAuthUserRepo(std::weak_ptr<IClient> client)
{
	const std::string request = std::string("user/repos");
	m_impl->DoRequest(std::move(client), request, RequestType::ListAuthUserRepo);
}

void Requester::GetRepoLang(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/languages";
	m_impl->DoRequest(std::move(client), request, RequestType::GetRepoLang);
}

void Requester::RepoContributors(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo)
{
	const std::string request = std::string("repos/") + owner + "/" + repo + "/contributors";
	m_impl->DoRequest(std::move(client), request, RequestType::RepoContributors);
}
