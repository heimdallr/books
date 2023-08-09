#include "GroupController.h"

#include <plog/Log.h>

#include "database/interface/ITransaction.h"

#include "interface/constants/Localization.h"
#include "interface/ui/IUiFactory.h"

#include "shared/DatabaseUser.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT = "GroupController";
constexpr auto INPUT_NEW_GROUP_NAME = QT_TRANSLATE_NOOP("GroupController", "Input new group name");
constexpr auto GROUP_NAME = QT_TRANSLATE_NOOP("GroupController", "Group name");
constexpr auto NEW_GROUP_NAME = QT_TRANSLATE_NOOP("GroupController", "New group");
constexpr auto REMOVE_GROUP_CONFIRM = QT_TRANSLATE_NOOP("GroupController", "Are you sure you want to delete the groups (%1)?");

constexpr auto CREATE_NEW_GROUP_QUERY = "insert into Groups_User(Title) values(?)";
constexpr auto REMOVE_GROUP_QUERY = "delete from Groups_User where GroupId = ?";
constexpr auto ADD_TO_GROUP_QUERY = "insert into Groups_List_User(GroupId, BookId) values(?, ?)";
constexpr auto REMOVE_FROM_GROUP_QUERY = "delete from Groups_List_User where BookID = ?";
constexpr auto REMOVE_FROM_GROUP_QUERY_SUFFIX = " and GroupID = ?";
constexpr auto SELECT_LAST_ID_QUERY = "select last_insert_rowid()";

long long CreateNewGroupImpl(DB::ITransaction & transaction, const QString & name)
{
	assert(!name.isEmpty());
	const auto command = transaction.CreateCommand(CREATE_NEW_GROUP_QUERY);
	command->Bind(0, name.toStdString());
	command->Execute();

	const auto query = transaction.CreateQuery(SELECT_LAST_ID_QUERY);
	query->Execute();
	return query->Get<long long>(0);
}

auto Tr(const char * str)
{
	return Loc::Tr(CONTEXT, str);
}

}

struct GroupController::Impl
{
	PropagateConstPtr<DatabaseUser, std::shared_ptr> databaseUser;
	PropagateConstPtr<IUiFactory, std::shared_ptr> uiFactory;

	explicit Impl(std::shared_ptr<DatabaseUser> databaseUser
		, std::shared_ptr<IUiFactory> uiFactory
	)
		: databaseUser(std::move(databaseUser))
		, uiFactory(std::move(uiFactory))
	{
	}
};

GroupController::GroupController(std::shared_ptr<DatabaseUser> databaseUser
	, std::shared_ptr<IUiFactory> uiFactory
)
	: m_impl(std::move(databaseUser), std::move(uiFactory))
{
	PLOGD << "GroupController created";
}

GroupController::~GroupController()
{
	PLOGD << "GroupController destroyed";
}

void GroupController::CreateNewGroup(Callback callback) const
{
	auto name = m_impl->uiFactory->GetText(Tr(INPUT_NEW_GROUP_NAME), Tr(GROUP_NAME), Tr(NEW_GROUP_NAME));
	if (name.isEmpty())
		return;

	m_impl->databaseUser->Execute({ "Create group", [&, name = std::move(name), callback = std::move(callback)]() mutable
	{
		const auto db = m_impl->databaseUser->Database();
		const auto transaction = db->CreateTransaction();
		CreateNewGroupImpl(*transaction, name);
		transaction->Commit();

		return [callback = std::move(callback)] (size_t)
		{
			callback();
		};
	} });
}

void GroupController::RemoveGroups(Ids ids, Callback callback) const
{
	if (false
		|| ids.empty()
		|| m_impl->uiFactory->ShowQuestion(Tr(REMOVE_GROUP_CONFIRM).arg(ids.size()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No
		)
		return;

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

void GroupController::AddToGroup(Id id, Ids ids, Callback callback) const
{
	auto name = [&]
	{
		if (id >= 0)
			return QString {};

		auto value = m_impl->uiFactory->GetText(Tr(INPUT_NEW_GROUP_NAME), Tr(GROUP_NAME), Tr(NEW_GROUP_NAME));
		return value.isEmpty() ? QString {} : value;
	}();

	m_impl->databaseUser->Execute({ "Add to group", [&, id, ids = std::move(ids), callback = std::move(callback), name = std::move(name)] () mutable
	{
		const auto db = m_impl->databaseUser->Database();
		const auto transaction = db->CreateTransaction();
		if (id < 0)
			id = CreateNewGroupImpl(*transaction, name);

		const auto command = transaction->CreateCommand(ADD_TO_GROUP_QUERY);
		std::ranges::for_each(ids, [&] (const Id idBook)
		{
			command->Bind(0, id);
			command->Bind(1, idBook);
			command->Execute();
		});
		transaction->Commit();

		return [callback = std::move(callback)] (size_t)
		{
			callback();
		};
	} });
}

void GroupController::RemoveFromGroup(Id id, Ids ids, Callback callback) const
{
	m_impl->databaseUser->Execute({ "Remove from group", [&, id, ids = std::move(ids), callback = std::move(callback)] () mutable
	{
		std::string queryText = REMOVE_FROM_GROUP_QUERY;
		if (id >= 0)
			queryText.append(REMOVE_FROM_GROUP_QUERY_SUFFIX);

		const auto db = m_impl->databaseUser->Database();
		const auto transaction = db->CreateTransaction();
		const auto command = transaction->CreateCommand(queryText);
		std::ranges::for_each(ids, [&] (const Id idBook)
		{
			command->Bind(0, idBook);
			if (id >= 0)
				command->Bind(1, id);

			command->Execute();
		});

		transaction->Commit();

		return [](size_t) { };
	} });
}