#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QQmlEngine>
#include <QTimer>

#include "database/interface/Command.h"
#include "database/interface/Database.h"
#include "database/interface/Query.h"
#include "database/interface/Transaction.h"

#include "constants/ObjectConnectorConstant.h"

#include "util/executor.h"

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

void AddBookToGroup(DB::Transaction & transaction, const std::vector<long long> & bookIds, const long long groupId)
{
	static constexpr auto queryText = "insert into Groups_List_User(BookID, GroupID) values(?, ?)";
	const auto command = transaction.CreateCommand(queryText);
	for (const auto bookId : bookIds)
	{
		command->Bind(1, bookId);
		command->Bind(2, groupId);
		command->Execute();
	}
}

void RemoveBookFromGroup(GroupsModelController & controller, long long bookId, long long groupId, Util::Executor & executor, DB::Database & db)
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
			command->Bind(1, bookId);
			if (groupId >= 0)
				command->Bind(2, groupId);

			command->Execute();
		}

		transaction->Commit();

		return [] { };
	} });
}

}

struct GroupsModelController::Impl
{
	Util::Executor & executor;
	DB::Database & db;

	PropagateConstPtr<QAbstractItemModel> addToModel { std::unique_ptr<QAbstractItemModel>(CreateSimpleModel({})) };
	PropagateConstPtr<QAbstractItemModel> removeFromModel { std::unique_ptr<QAbstractItemModel>(CreateSimpleModel({})) };

	long long bookId { -1 };

	QTimer checkTimer;
	bool checkNewNameInProgress { false };
	QString checkName;

	QString errorText;

	Impl(GroupsModelController & self, Util::Executor & executor_, DB::Database & db_)
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
			query->Bind(1, name);
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

GroupsModelController::GroupsModelController(Util::Executor & executor, DB::Database & db, QObject * parent)
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
			"from Groups_User g left join Groups_List_User gl on gl.GroupID = g.GroupID and gl.BookID = ?"
			;

		const auto query = m_impl->db.CreateQuery(queryText);
		query->Bind(1, m_impl->bookId);

		SimpleModelItems addTo, removeFrom;

		for (query->Execute(); !query->Eof(); query->Next())
		{
			SimpleModelItem item { query->Get<const char *>(0), query->Get<const char *>(1) };
			(query->Get<int>(2) < 0 ? addTo : removeFrom).push_back(std::move(item));
		}

		return [addTo = std::move(addTo), removeFrom = std::move(removeFrom), &impl = *m_impl] () mutable
		{
			impl.addToModel->setData({}, QVariant::fromValue(&addTo), SimpleModelRole::SetItems);
			impl.removeFromModel->setData({}, QVariant::fromValue(&removeFrom), SimpleModelRole::SetItems);
		};
	} });
}

void GroupsModelController::AddToNew(const QString & name)
{
	m_impl->executor({ "Add to new group", [this, name, bookIds = GetCheckedBooksIds(*this, m_impl->bookId)]
	{
		static constexpr auto getMaxIdQueryText = "select last_insert_rowid()";
		static constexpr auto insertText = "insert into Groups_User(Title) values(?)";

		const auto transaction = m_impl->db.CreateTransaction();

		const auto command = transaction->CreateCommand(insertText);
		command->Bind(1, name.toStdString());
		command->Execute();

		const auto query = transaction->CreateQuery(getMaxIdQueryText);
		query->Execute();
		const auto id = query->Get<long long>(0);

		AddBookToGroup(*transaction, bookIds, id);

		transaction->Commit();

		return [] {};
	} });
}

void GroupsModelController::AddTo(const QString & id)
{
	m_impl->executor({ "Add to group", [this, id = id.toLongLong(), bookIds = GetCheckedBooksIds(*this, m_impl->bookId)]
	{
		const auto transaction = m_impl->db.CreateTransaction();
		AddBookToGroup(*transaction, bookIds, id);
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

bool GroupsModelController::IsCheckNewNameInProgress() const noexcept
{
	return m_impl->checkNewNameInProgress;
}

const QString & GroupsModelController::GetErrorText() const noexcept
{
	return m_impl->errorText;
}

}
