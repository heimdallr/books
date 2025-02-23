#pragma once

#include <string>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "network/rest/connection/IConnection.h"

#include "export/rest.h"

namespace HomeCompa::RestAPI::Github
{

struct Release;

class IClient // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IClient() = default;

public:
	virtual void HandleLatestRelease(const Release& /*release*/)
	{
	}
};

class REST_EXPORT Requester
{
	NON_COPY_MOVABLE(Requester)

public:
	explicit Requester(std::unique_ptr<IConnection> connection);
	~Requester();

	/**
	 * @brief Request user info
	 * @param client request result receiver
	 * @param user GitHub user name
	 *
	 * Request user information. Equivalent of fetching https://api.github.com/users/\<user>
	 */
	void GetUserInfo(std::weak_ptr<IClient> client, const std::string& user);

	/**
	 * @brief Request releases for repository
	 * @param client request result receiver
	 * @param user GitHub user name
	 * @param repo user's repository name
	 *
	 * Request list of releases for repository. Equivalent of fetching https://api.github.com/repos/\<user>/\<repo>/releases
	 */
	void ListReleases(std::weak_ptr<IClient> client, const std::string& user, const std::string& repo);

	/**
	 * @brief Request latest release
	 * @param client request result receiver
	 * @param user GitHub user name
	 * @param repo user's repository name
	 *
	 * Request details of latest release. Equivalent of fetching https://api.github.com/repos/<user>/<repo>/releases/latest
	 */
	void GetLatestRelease(std::weak_ptr<IClient> client, const std::string& user, const std::string& repo);

	/**
	 * @brief Request release details
	 * @param client request result receiver
	 * @param user GitHub user name
	 * @param repo user's repository name
	 * @param id release id. Id is returned as a part of \ref getReleases
	 *
	 * Request details of release. Equivalent of fetching https://api.github.com/repos/\<user>/\<repo>/releases/\<id>
	 */

	void GetRelease(std::weak_ptr<IClient> client, const std::string& user, const std::string& repo, int id);
	/**
	 * @brief Request api limits
	 * @param client request result receiver
	 *
	 * Request limits for api calls. Equivalent of fetching https://api.github.com/rate_limit
	 */
	void GetRateLimit(std::weak_ptr<IClient> client);

	/**
	 * @brief Request list of user repositories
	 * @param client request result receiver
	 * @param user GitHub user name
	 *
	 * Request list of repositories for user. Equivalent of fetching https://api.github.com/users/\<user>/repos
	 */
	void ListUserRepo(std::weak_ptr<IClient> client, const std::string& user);

	// --------------- user info related api

	/**
	 * @brief get the authenticated user info
	 * @param client request result receiver
	 */
	void GetAuthenticatedUser(std::weak_ptr<IClient> client);

	/**
	 * @brief Lists all users, in the order that they
	 * @param client request result receiver
	 * signed up on GitHub. This list includes personal
	 * user accounts and organization accounts.
	 */
	void ListUsers(std::weak_ptr<IClient> client);

	/**
	 * @brief Provides publicly available information about
	 * someone with a GitHub account.
	 * @param client request result receiver
	 *
	 * @param username github user name
	 */
	void GetUser(std::weak_ptr<IClient> client, const std::string& username);

	// issues related api methods
	/**
	 * @brief List issues assigned to the authenticated user
	 * across all visible repositories including owned
	 * repositories, member repositories, and organization
	 * repositories.
	 * @param client request result receiver
	 *
	 */
	void ListIssues(std::weak_ptr<IClient> client);

	/**
	 * @brief List issues in an organization assigned to the
	 * authenticated user.
	 *
	 * @param client request result receiver
	 * @param org github organization
	 */
	void ListOrgIssues(std::weak_ptr<IClient> client, const std::string& org);

	/**
	 * @brief List issues in a repository.
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo The name of the repository.
	 */
	void ListRepoIssues(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief Lists details of a issue by providing its number.
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository.
	 * @param issueNumber The number that identifies the issue.
	 */
	void GetIssue(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& issueNumber);

	// pull request related api methods
	/**
	 * @brief List pull request in a repository.
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo The name of the repository.
	 */
	void ListPullRequest(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief Lists details of a pull request by providing its number.
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository.
	 * @param pullNumber The number that identifies the PR.
	 */
	void GetPullRequest(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& pullNumber);

	/**
	 * @brief Lists a maximum of 250 commits for a pull request
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository
	 * @param pullNumber The number that identifies the PR.
	 */
	void ListPullRequestCommit(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& pullNumber);

	/**
	 * @brief Responses include a maximum of 3000 files.
	 *      The paginated response returns 30 files per
	 *      page by default.
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository
	 * @param pullNumber The number that identifies the PR.
	 */
	void ListPullRequestFiles(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& pullNumber);

	/**
	 * @brief Check if a pull request has been merged
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository
	 * @param pullNumber The number that identifies the PR.
	 */
	void IsPullRequestMerged(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& pullNumber);

	// commits related api methods
	/**
	 * @brief List commits.
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo The name of the repository.
	 */
	void ListCommits(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief List branches for HEAD commit
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository
	 * @param commitSha The SHA of the commit.
	 */
	void ListBranchHeadCommit(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& commitSha);

	/**
	 * @brief List pull requests associated with a commit
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository
	 * @param commitSha The SHA of the commit.
	 */
	void ListCommitPullRequest(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& commitSha);

	/**
	 * @brief Get a commit
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo The name of the repository.
	 * @param reference ref parameter
	 */
	void GetCommits(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo, const std::string& reference);

	// metrics related api methods
	/**
	 * @brief Get the weekly commit activity
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository.
	 */
	void GetWeeklyCommit(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief Get the last year of commit activity
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository.
	 */
	void GetLastYearCommit(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief Get all contributor commit activity
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository.
	 * Returns the total number of commits authored by the
	 *         contributor. In addition, the response includes a Weekly
	 *         Hash (weeks array) with the following information:
			w - Start of the week, given as a Unix timestamp.
			a - Number of additions
			d - Number of deletions
			c - Number of commits
	*/
	void GetContributorsActivity(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief Get the weekly commit count
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository.
	 * The total commit counts for the owner and total commit counts in
	 *         all. All is everyone combined, including the owner in the last 52
	 *         weeks. If you'd like to get the commit counts for non-owners, you
	 *         can subtract owner from all.
	 */
	void GetCommitCount(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief Get the hourly commit count for each day
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository.
	 * Returns array containing the day number, hour number,
	 *         and number of commits:
	 *         For example, [2, 14, 25] indicates that there
	 *         were 25 total commits, during the 2:00pm hour on Tuesdays
	 */
	void GetHourlyCommitCount(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief Get community profile metrics
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository.
	 * all community profile metrics, including an overall health
	 *         score, repository description, the presence of documentation
	 *         detected code of conduct, detected license,and the presence of
	 *         ISSUE_TEMPLATE, PULL_REQUEST_TEMPLATE,README, and CONTRIBUTING
	 *         files
	 */
	void GetCommunityProfileMetrics(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief Get repository clones
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository.
	 * return Get the total number of clones and breakdown per day
	 *         or week for the last 14 days. Timestamps are aligned to UTC
	 *         midnight of the beginning of the day or week. Week begins on
	 *         Monday.
	 */
	void GetRepoClones(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief Get top referral paths
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo the name of the repository.
	 * return lists of the top 10 popular contents over the last
	 *         14 days.
	 */
	void GetReferralPaths(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief Get top referral sources
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo The name of the repository.
	 * Get the top 10 referrers over the last 14 days.
	 */
	void GetTopReferralSource(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief Get page views. Get the total number of views and breakdown
	 *        per day or week for the last 14 days.
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo The name of the repository.
	 * Get the top 10 referrers over the last 14 days.
	 */
	void GetPageViews(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	// event related apis
	/**
	 * @brief List public events for a network of
	 *        repositories
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo The name of the repository.
	 * public event associated with repo
	 */
	void ListNetworkRepoEvent(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief List public organization events
	 *
	 * @param client request result receiver
	 * @param org The organization name.
	 * return public event associated with repo
	 */
	void ListOrgEvent(std::weak_ptr<IClient> client, const std::string& org);

	/**
	 * @brief List repository events
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo The name of the repository.
	 * return public event associated with repo
	 */
	void ListRepoEvent(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief List events for the authenticated user
	 *
	 * @param client request result receiver
	 * @param username The handle for the GitHub user account
	 * return If you are authenticated as the given user, you will see your
	 *         private events. Otherwise, you'll only see public events.
	 */
	void ListUserEvent(std::weak_ptr<IClient> client, const std::string& username);

	// staring related api methods

	/**
	 * @brief Lists the people that have starred the repository.
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository.
	 * @param repo The name of the repository.
	 * returns Lists of people that have starred the repository.
	 */
	void ListStargazers(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief List repositories starred by a user
	 *
	 * @param client request result receiver
	 * @param username The handle for the GitHub user account
	 * returns a list of repositories a user has starred
	 */
	void ListUserStarredRepo(std::weak_ptr<IClient> client, const std::string& username);

	// watching related api

	/**
	 * @brief Lists the people watching the specified repository.
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository
	 * @param repo The name of the repository
	 * return list of github account watching the repository
	 */
	void ListRepoWatchers(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief Get a repository subscription.
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository
	 * @param repo The name of the repository
	 * return list of users subscribe to the repository
	 */
	void GetRepoSubscription(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief List repositories watched by a user
	 *
	 * @param client request result receiver
	 * @param username The handle for the GitHub user account
	 * return Lists repositories a user is watching.
	 */
	void ListUserWatchedRepos(std::weak_ptr<IClient> client, const std::string& username);

	/**
	 * @brief List repository collaborators
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository
	 * @param repo The name of the repository
	 * return  list of collaborators includes outside collaborators,
	 *          organization members that are direct collaborators,
	 *          organization members with access through team memberships,
	 *          organization members with access through default
	 *          organization permissions, and organization owners
	 */
	void ListRepoCollaborators(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief List organization repositories
	 *
	 * @param client request result receiver
	 * @param org The organization name
	 * return Lists repositories for the specified organization.
	 */
	void GetOrgRepo(std::weak_ptr<IClient> client, const std::string& org);

	/**
	 * @brief Get a repository
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository
	 * @param repo The name of the repository
	 * return The parent and source objects are present
	 *          when the repository is a fork. parent is
	 *          the repository this repository was forked from,
	 *          source is the ultimate source for the network.
	 */
	void GetRepository(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief List repositories for the authenticated user
	 *
	 * @param client request result receiver
	 * return Lists repositories that the authenticated user
	 *         has explicit permission (:read, :write, or :admin)
	 *         to access.
	 */
	void ListAuthUserRepo(std::weak_ptr<IClient> client);

	/**
	 * @brief List repository languages
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository
	 * @param repo The name of the repository
	 * return Lists languages for the specified repository.
	 *         The value shown for each language is the number
	 *         of bytes of code written in that language.
	 */
	void GetRepoLang(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

	/**
	 * @brief List repository contributors
	 *
	 * @param client request result receiver
	 * @param owner The account owner of the repository
	 * @param repo The name of the repository
	 * return Lists contributors to the specified repository
	 *         and sorts them by the number of commits per
	 *         contributor in descending order
	 */
	void RepoContributors(std::weak_ptr<IClient> client, const std::string& owner, const std::string& repo);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::RestAPI::Github
