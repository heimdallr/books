#include "GroupController.h"

#include "database/interface/ITransaction.h"

#include "shared/DatabaseUser.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CREATE_NEW_GROUP_QUERY = "insert into Groups_User(Title) values(?)";
constexpr auto REMOVE_GROUP_QUERY = "delete from Groups_User where GroupId = ?";

}

struct GroupController::Impl
{
	PropagateConstPtr<DatabaseUser, std::shared_ptr> databaseUser;

	explicit Impl(std::shared_ptr<DatabaseUser> databaseUser)
		: databaseUser(std::move(databaseUser))
	{
	}
};

GroupController::GroupController(std::shared_ptr<DatabaseUser> databaseUser)
	: m_impl(std::move(databaseUser))
{
}

GroupController::~GroupController() = default;

void GroupController::CreateNewGroup(QString name, Callback callback) const
{
	m_impl->databaseUser->Execute({ "Create group", [&, name = std::move(name), callback = std::move(callback)]() mutable
	{
		const auto db = m_impl->databaseUser->Database();
		const auto transaction = db->CreateTransaction();
		const auto command = transaction->CreateCommand(CREATE_NEW_GROUP_QUERY);
		command->Bind(0, name.toStdString());
		command->Execute();
		transaction->Commit();

		return [callback = std::move(callback)] (size_t)
		{
			callback();
		};
	} });
}

void GroupController::RemoveGroups(Ids ids, Callback callback) const
{
	m_impl->databaseUser->Execute({ "Remove groups", [&, ids = std::move(ids), callback = std::move(callback)] () mutable
	{
		const auto db = m_impl->databaseUser->Database();
		const auto transaction = db->CreateTransaction();
		const auto command = transaction->CreateCommand(REMOVE_GROUP_QUERY);

		for (const auto & id : ids)
		{
			command->Bind(0, id);
			command->Execute();
		}
		transaction->Commit();

		return [callback = std::move(callback)] (size_t)
		{
			callback();
		};
	} });
}
