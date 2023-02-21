#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QQmlEngine>
#include <QTimer>

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "constants/ObjectConnectorConstant.h"

#include "util/IExecutor.h"

#include "constants/UserData/groups.h"
#include "constants/UserData/UserData.h"

#include "models/Book.h"
#include "models/SimpleModel.h"

#include "GroupsModelController.h"

namespace HomeCompa::Flibrary {

namespace {

std::vector<long long> GetCheckedBooksIds(GroupsModelController & controller, long long id)
{
	Books books;
	emit controller.GetCheckedBooksRequest(books);

	std::vector<long long> ids;
	std::ranges::transform(std::as_const(books), std::back_inserter(ids), [] (const Book & book)
	{
		return book.Id;
	});
	if (ids.empty())
		ids.push_back(id);

	return ids;
}

void AddBookToGroupImpl(DB::ITransaction & transaction, const std::vector<long long> & bookIds, const long long groupId)
{
	const auto command = transaction.CreateCommand(Constant::UserData::Groups::AddBookToGroupCommandText);
	for (const auto bookId : bookIds)
	{
		command->Bind(0, bookId);
		command->Bind(1, groupId);
		command->Execute();
	}
}

long long CreateNewGroupImpl(DB::ITransaction & transaction, const QString & name)
{
	const auto command = transaction.CreateCommand(Constant::UserData::Groups::CreateNewGroupCommandText);
	command->Bind(0, name.toStdString());
	command->Execute();

	const auto query = transaction.CreateQuery(Constant::UserData::SelectLastIdQueryText);
	query->Execute();
	return query->Get<long long>(0);
}

void RemoveBookFromGroup(GroupsModelController & controller, long long bookId, long long groupId, Util::IExecutor & executor, DB::IDatabase & db)
{
	executor({ "Remove from group", [&db, groupId, bookIds = GetCheckedBooksIds(controller, bookId)]
	{
		std::string queryText = "delete from Groups_List_User where BookID = ?";
		if (groupId >= 0)
			queryText.append(" and GroupID = ?");

		const auto transaction = db.CreateTransaction();
		const auto command = transaction->CreateCommand(queryText);
		for (const auto bookId : bookIds)
		{
			command->Bind(0, bookId);
			if (groupId >= 0)
				command->Bind(1, groupId);

			command->Execute();
		}

		transaction->Commit();

		return [] { };
	} });
}

}

struct GroupsModelController::Impl
{
	Util::IExecutor & executor;
	DB::IDatabase & db;

	PropagateConstPtr<QAbstractItemModel> addToModel { std::unique_ptr<QAbstractItemModel>(CreateSimpleModel({})) };
	PropagateConstPtr<QAbstractItemModel> removeFromModel { std::unique_ptr<QAbstractItemModel>(CreateSimpleModel({})) };

	long long bookId { -1 };

	bool checkNewNameInProgress { false };
	bool toAddExists { false };

	QTimer checkTimer;
	QString checkName;

	QString errorText;

	Impl(GroupsModelController & self, Util::IExecutor & executor_, DB::IDatabase & db_)
		: executor(executor_)
		, db(db_)
		, m_self(self)
	{
		QQmlEngine::setObjectOwnership(addToModel.get(), QQmlEngine::CppOwnership);
		QQmlEngine::setObjectOwnership(removeFromModel.get(), QQmlEngine::CppOwnership);

		checkTimer.setSingleShot(true);
		checkTimer.setInterval(std::chrono::milliseconds(300));
		connect(&checkTimer, &QTimer::timeout, [&] { Check(); });
	}

private:
	void Check()
	{
		executor({ "Check new group name",[this, name = checkName.toUpper().toStdString()]
		{
			const auto query = db.CreateQuery("select GroupID from Groups_User where Title = ?");
			query->Bind(0, name);
			query->Execute();
			const auto id = query->Get<int>(0);

			return [this, id]
			{
				errorText = id > 0 ? QCoreApplication::translate("GroupsModel", "A group with the same name already exists") : "";
				emit m_self.ErrorTextChanged();

				checkNewNameInProgress = false;
				emit m_self.CheckNewNameInProgressChanged();
			};
		} });
	}

private:
	GroupsModelController & m_self;
};

GroupsModelController::GroupsModelController(Util::IExecutor & executor, DB::IDatabase & db, QObject * parent)
	: QObject(parent)
	, m_impl(*this, executor, db)
{
	Util::ObjectsConnector::registerEmitter(ObjConn::GET_CHECKED_BOOKS, this, SIGNAL(GetCheckedBooksRequest(std::vector<Book> &)));
}

GroupsModelController::~GroupsModelController() = default;

QAbstractItemModel * GroupsModelController::GetAddToModel()
{
	return m_impl->addToModel.get();
}

QAbstractItemModel * GroupsModelController::GetRemoveFromModel()
{
	return m_impl->removeFromModel.get();
}

void GroupsModelController::Reset(long long bookId)
{
	m_impl->bookId = bookId;
	m_impl->executor({ "Get groups", [this]
	{
		static constexpr auto queryText =
			"select g.GroupID, g.Title, coalesce(gl.BookID, -1) "
			"from Groups_User g left join Groups_List_User gl on gl.GroupID = g.GroupID and gl.BookID = ? "
			"order by g.Title"
			;

		const auto query = m_impl->db.CreateQuery(queryText);
		query->Bind(0, m_impl->bookId);

		SimpleModelItems addTo, removeFrom;

		for (query->Execute(); !query->Eof(); query->Next())
		{
			SimpleModelItem item { query->Get<const char *>(0), query->Get<const char *>(1) };
			(query->Get<int>(2) < 0 ? addTo : removeFrom).push_back(std::move(item));
		}

		return [this, addTo = std::move(addTo), removeFrom = std::move(removeFrom)]() mutable
		{
			m_impl->addToModel->setData({}, QVariant::fromValue(&addTo), SimpleModelRole::SetItems);
			m_impl->removeFromModel->setData({}, QVariant::fromValue(&removeFrom), SimpleModelRole::SetItems);

			m_impl->toAddExists = !addTo.empty();
			emit ToAddExistsChanged();
		};
	} });
}

void GroupsModelController::AddToNew(const QString & name)
{
	m_impl->executor({ "Add to new group", [this, name, bookIds = GetCheckedBooksIds(*this, m_impl->bookId)]
	{
		const auto transaction = m_impl->db.CreateTransaction();
		const auto id = CreateNewGroupImpl(*transaction, name);
		AddBookToGroupImpl(*transaction, bookIds, id);
		transaction->Commit();
		return [] {};
	} });
}

void GroupsModelController::AddTo(const QString & id)
{
	m_impl->executor({ "Add to group", [this, id = id.toLongLong(), bookIds = GetCheckedBooksIds(*this, m_impl->bookId)]
	{
		const auto transaction = m_impl->db.CreateTransaction();
		AddBookToGroupImpl(*transaction, bookIds, id);
		transaction->Commit();
		return [] {};
	} });
}

void GroupsModelController::RemoveFrom(const QString & id)
{
	RemoveBookFromGroup(*this, m_impl->bookId, id.toLongLong(), m_impl->executor, m_impl->db);
}

void GroupsModelController::RemoveFromAll()
{
	RemoveBookFromGroup(*this, m_impl->bookId, -1, m_impl->executor, m_impl->db);
}

void GroupsModelController::CheckNewName(const QString & name)
{
	if (name.isEmpty())
		return;

	if (name.length() > 50)
	{
		m_impl->errorText = QCoreApplication::translate("GroupsModel", "Group name too long");
		return emit ErrorTextChanged();
	}

	m_impl->checkNewNameInProgress = true;
	emit CheckNewNameInProgressChanged();

	m_impl->checkName = name;
	m_impl->checkTimer.start();
}

void GroupsModelController::CreateNewGroup(const QString & name)
{
	m_impl->executor({ "Create new group", [this, name]
	{
		const auto transaction = m_impl->db.CreateTransaction();
		CreateNewGroupImpl(*transaction, name);
		transaction->Commit();
		return [] {};
	} });
}

void GroupsModelController::RemoveGroup(long long groupId)
{
	m_impl->executor({ "Remove group", [this, groupId]
	{
		const auto transaction = m_impl->db.CreateTransaction();
		const auto command = transaction->CreateCommand("delete from Groups_User where GroupId = ?");
		command->Bind(0, groupId);
		command->Execute();
		transaction->Commit();
		return [] {};
	} });
}

bool GroupsModelController::IsCheckNewNameInProgress() const noexcept
{
	return m_impl->checkNewNameInProgress;
}

bool GroupsModelController::IsToAddExists() const noexcept
{
	return m_impl->toAddExists;
}

const QString & GroupsModelController::GetErrorText() const noexcept
{
	return m_impl->errorText;
}

}
