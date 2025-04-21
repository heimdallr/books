#include "GroupController.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "GroupController";
constexpr auto INPUT_NEW_GROUP_NAME = QT_TRANSLATE_NOOP("GroupController", "Input new group name");
constexpr auto GROUP_NAME = QT_TRANSLATE_NOOP("GroupController", "Group name");
constexpr auto NEW_GROUP_NAME = QT_TRANSLATE_NOOP("GroupController", "New group");
constexpr auto REMOVE_GROUP_CONFIRM = QT_TRANSLATE_NOOP("GroupController", "Are you sure you want to delete the groups (%1)?");
constexpr auto GROUP_NAME_TOO_LONG = QT_TRANSLATE_NOOP("GroupController", "Group name too long.\nTry again?");
constexpr auto GROUP_ALREADY_EXISTS = QT_TRANSLATE_NOOP("GroupController", "Group %1 already exists.\nTry again?");
constexpr auto CANNOT_CREATE_GROUP = QT_TRANSLATE_NOOP("GroupController", "Cannot create group");
constexpr auto CANNOT_RENAME_GROUP = QT_TRANSLATE_NOOP("GroupController", "Cannot rename group");
constexpr auto CANNOT_REMOVE_GROUPS = QT_TRANSLATE_NOOP("GroupController", "Cannot remove groups");
constexpr auto CANNOT_ADD_BOOK_TO_GROUP = QT_TRANSLATE_NOOP("GroupController", "Cannot add book to group");
constexpr auto CANNOT_REMOVE_BOOKS_FROM_GROUP = QT_TRANSLATE_NOOP("GroupController", "Cannot remove books from group");

constexpr auto CREATE_NEW_GROUP_QUERY = "insert into Groups_User(Title, CreatedAt) values(?, datetime(CURRENT_TIMESTAMP, 'localtime'))";
constexpr auto REMOVE_GROUP_QUERY = "delete from Groups_User where GroupId = ?";
constexpr auto ADD_TO_GROUP_QUERY = "insert into Groups_List_User(GroupId, BookId, CreatedAt) values(?, ?, datetime(CURRENT_TIMESTAMP, 'localtime'))";
constexpr auto REMOVE_FROM_GROUP_QUERY = "delete from Groups_List_User where BookID = ?";
constexpr auto REMOVE_FROM_GROUP_QUERY_SUFFIX = " and GroupID = ?";

using Names = std::unordered_set<QString>;

long long CreateNewGroupImpl(DB::ITransaction& transaction, const QString& name)
{
	assert(!name.isEmpty());
	const auto command = transaction.CreateCommand(CREATE_NEW_GROUP_QUERY);
	command->Bind(0, name.toStdString());
	if (!command->Execute())
		return 0;

	const auto query = transaction.CreateQuery(IDatabaseUser::SELECT_LAST_ID_QUERY);
	query->Execute();
	return query->Get<long long>(0);
}

TR_DEF

} // namespace

struct GroupController::Impl
{
	std::shared_ptr<const IDatabaseUser> databaseUser;
	PropagateConstPtr<INavigationQueryExecutor, std::shared_ptr> navigationQueryExecutor;
	std::shared_ptr<const IUiFactory> uiFactory;

	explicit Impl(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<INavigationQueryExecutor> navigationQueryExecutor, std::shared_ptr<const IUiFactory> uiFactory)
		: databaseUser(std::move(databaseUser))
		, navigationQueryExecutor(std::move(navigationQueryExecutor))
		, uiFactory(std::move(uiFactory))
	{
	}

	void GetAllGroups(std::function<void(const Names&)> callback) const
	{
		navigationQueryExecutor->RequestNavigation(NavigationMode::Groups,
		                                           [callback = std::move(callback)](NavigationMode, const IDataItem::Ptr& root)
		                                           {
													   Names names;
													   for (size_t i = 0, sz = root->GetChildCount(); i < sz; ++i)
														   names.insert(root->GetChild(i)->GetData());
													   callback(names);
												   });
	}

	void CreateNewGroup(const Names& names, Callback callback) const
	{
		auto name = GetNewGroupName(names);
		if (name.isEmpty())
			return;

		databaseUser->Execute({ "Create group",
		                        [&, name = std::move(name), callback = std::move(callback)]() mutable
		                        {
									const auto db = databaseUser->Database();
									const auto transaction = db->CreateTransaction();
									auto id = CreateNewGroupImpl(*transaction, name);
									auto ok = id > 0;
									ok = transaction->Commit() && ok;

									return [this, id, callback = std::move(callback), ok](size_t)
									{
										if (!ok)
											uiFactory->ShowError(Tr(CANNOT_CREATE_GROUP));

										callback(id);
									};
								} });
	}

	void RenameGroup(const Id id, QString name, const Names& names, Callback callback) const
	{
		name = GetNewGroupName(names, std::move(name));
		if (name.isEmpty())
			return;

		databaseUser->Execute({ "Rename group",
		                        [&, id, name = std::move(name), callback = std::move(callback)]() mutable
		                        {
									const auto db = databaseUser->Database();
									const auto transaction = db->CreateTransaction();
									const auto command = transaction->CreateCommand("update Groups_User set Title = ? where GroupID = ?");
									command->Bind(0, name.toStdString());
									command->Bind(1, id);
									auto ok = command->Execute();
									ok = transaction->Commit() && ok;

									return [this, id, callback = std::move(callback), ok](size_t)
									{
										if (!ok)
											uiFactory->ShowError(Tr(CANNOT_RENAME_GROUP));

										callback(id);
									};
								} });
	}

	void AddToGroup(Id id, Ids ids, QString name, Callback callback) const
	{
		databaseUser->Execute({ "Add to group",
		                        [&, id, ids = std::move(ids), callback = std::move(callback), name = std::move(name)]() mutable
		                        {
									const auto db = databaseUser->Database();
									const auto transaction = db->CreateTransaction();

									auto errorMessage = std::make_shared<QString>();

									if (id < 0)
										id = CreateNewGroupImpl(*transaction, name);

									std::function result = [this, id, callback = std::move(callback), errorMessage](size_t)
									{
										if (!errorMessage->isEmpty())
											uiFactory->ShowError(*errorMessage);

										callback(id);
									};

									if (id == 0)
									{
										*errorMessage = Tr(CANNOT_CREATE_GROUP);
										return result;
									}

									const auto command = transaction->CreateCommand(ADD_TO_GROUP_QUERY);
									bool ok = true;
									std::ranges::for_each(ids,
			                                              [&](const Id idBook)
			                                              {
															  command->Bind(0, id);
															  command->Bind(1, idBook);
															  ok = command->Execute() && ok;
														  });
									ok = transaction->Commit() && ok;

									if (!ok)
										*errorMessage = Tr(CANNOT_ADD_BOOK_TO_GROUP);

									return result;
								} });
	}

	QString GetNewGroupName(const Names& names, QString name = {}) const
	{
		while (true)
		{
			if (name.isEmpty())
				name = Tr(NEW_GROUP_NAME);

			auto newName = name;
			for (int i = 2; names.contains(newName); ++i)
				newName = QString("%1 %2").arg(name).arg(i);

			name = uiFactory->GetText(Tr(INPUT_NEW_GROUP_NAME), Tr(GROUP_NAME), newName);
			if (name.isEmpty())
				return {};

			if (names.contains(name))
			{
				if (uiFactory->ShowWarning(Tr(GROUP_ALREADY_EXISTS).arg(name), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
					continue;

				return {};
			}

			if (name.length() > 148)
			{
				if (uiFactory->ShowWarning(Tr(GROUP_NAME_TOO_LONG), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
					continue;

				return {};
			}

			return name;
		}
	}
};

GroupController::GroupController(std::shared_ptr<IDatabaseUser> databaseUser, std::shared_ptr<INavigationQueryExecutor> navigationQueryExecutor, std::shared_ptr<IUiFactory> uiFactory)
	: m_impl(std::move(databaseUser), std::move(navigationQueryExecutor), std::move(uiFactory))
{
	PLOGV << "GroupController created";
}

GroupController::~GroupController()
{
	PLOGV << "GroupController destroyed";
}

void GroupController::CreateNew(Callback callback) const
{
	m_impl->GetAllGroups([&, callback = std::move(callback)](const Names& names) mutable { m_impl->CreateNewGroup(names, std::move(callback)); });
}

void GroupController::Rename(const Id id, QString name, Callback callback) const
{
	m_impl->GetAllGroups([&, id, name = std::move(name), callback = std::move(callback)](const Names& names) mutable { m_impl->RenameGroup(id, std::move(name), names, std::move(callback)); });
}

void GroupController::Remove(Ids ids, Callback callback) const
{
	if (ids.empty() || m_impl->uiFactory->ShowQuestion(Tr(REMOVE_GROUP_CONFIRM).arg(ids.size()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
		return;

	m_impl->databaseUser->Execute({ "Remove groups",
	                                [&, ids = std::move(ids), callback = std::move(callback)]() mutable
	                                {
										const auto db = m_impl->databaseUser->Database();
										const auto transaction = db->CreateTransaction();
										const auto command = transaction->CreateCommand(REMOVE_GROUP_QUERY);

										auto ok = std::accumulate(ids.cbegin(),
		                                                          ids.cend(),
		                                                          true,
		                                                          [&](const bool init, const Id id)
		                                                          {
																	  command->Bind(0, id);
																	  return command->Execute() && init;
																  });
										ok = transaction->Commit() && ok;

										return [this, callback = std::move(callback), ok](size_t)
										{
											if (!ok)
												m_impl->uiFactory->ShowError(Tr(CANNOT_REMOVE_GROUPS));

											callback(-1);
										};
									} });
}

void GroupController::AddToGroup(const Id id, Ids ids, Callback callback) const
{
	if (id >= 0)
		return m_impl->AddToGroup(id, std::move(ids), {}, std::move(callback));

	m_impl->GetAllGroups(
		[&, ids = std::move(ids), callback = std::move(callback)](const Names& names) mutable
		{
			auto name = m_impl->GetNewGroupName(names);
			if (name.isEmpty())
				return callback(id);

			m_impl->AddToGroup(id, std::move(ids), std::move(name), std::move(callback));
		});
}

void GroupController::RemoveFromGroup(Id id, Ids ids, Callback callback) const
{
	m_impl->databaseUser->Execute({ "Remove from group",
	                                [&, id, ids = std::move(ids), callback = std::move(callback)]() mutable
	                                {
										std::string queryText = REMOVE_FROM_GROUP_QUERY;
										if (id >= 0)
											queryText.append(REMOVE_FROM_GROUP_QUERY_SUFFIX);

										const auto db = m_impl->databaseUser->Database();
										const auto transaction = db->CreateTransaction();
										const auto command = transaction->CreateCommand(queryText);
										auto ok = std::accumulate(ids.cbegin(),
		                                                          ids.cend(),
		                                                          true,
		                                                          [&](const bool init, const Id idBook)
		                                                          {
																	  command->Bind(0, idBook);
																	  if (id >= 0)
																		  command->Bind(1, id);

																	  return command->Execute() && init;
																  });
										ok = transaction->Commit() && ok;

										return [this, id, callback = std::move(callback), ok](size_t)
										{
											if (!ok)
												m_impl->uiFactory->ShowError(Tr(CANNOT_REMOVE_BOOKS_FROM_GROUP));

											callback(id);
										};
									} });
}
