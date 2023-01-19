#include <QAbstractItemModel>
#include <QQmlEngine>

#include "database/interface/Command.h"
#include "database/interface/Database.h"
#include "database/interface/Query.h"
#include "database/interface/Transaction.h"

#include "util/executor.h"

#include "models/SimpleModel.h"

#include "GroupsModelController.h"

#include <QCoreApplication>

namespace HomeCompa::Flibrary {

namespace {

void AddBookToGroup(DB::Transaction & transaction, const long long bookId, const long long groupId)
{
	static constexpr auto queryText = "insert into Groups_List_User(BookID, GroupID) values(?, ?)";
	const auto command = transaction.CreateCommand(queryText);
	command->Bind(1, bookId);
	command->Bind(2, groupId);
	command->Execute();
}

}

struct GroupsModelController::Impl
{
	Util::Executor & executor;
	DB::Database & db;

	PropagateConstPtr<QAbstractItemModel> addToModel { std::unique_ptr<QAbstractItemModel>(CreateSimpleModel({})) };
	PropagateConstPtr<QAbstractItemModel> removeFromModel { std::unique_ptr<QAbstractItemModel>(CreateSimpleModel({})) };

	long long bookId { -1 };

	Impl(Util::Executor & executor_, DB::Database & db_)
		: executor(executor_)
		, db(db_)
	{
		QQmlEngine::setObjectOwnership(addToModel.get(), QQmlEngine::CppOwnership);
		QQmlEngine::setObjectOwnership(removeFromModel.get(), QQmlEngine::CppOwnership);
	}
};

GroupsModelController::GroupsModelController(Util::Executor & executor, DB::Database & db, QObject * parent)
	: QObject(parent)
	, m_impl(executor, db)
{
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

		addTo.emplace_back("-1", QCoreApplication::translate("GroupsModel", "New group..."));
		if (!removeFrom.empty())
			removeFrom.emplace_back("-1", QCoreApplication::translate("GroupsModel", "All"));

		return [addTo = std::move(addTo), removeFrom = std::move(removeFrom), &impl = *m_impl] () mutable
		{
			impl.addToModel->setData({}, QVariant::fromValue(&addTo), SimpleModelRole::SetItems);
			impl.removeFromModel->setData({}, QVariant::fromValue(&removeFrom), SimpleModelRole::SetItems);
		};
	} });
}

void GroupsModelController::AddToNew(const QString & name)
{
	m_impl->executor({ "Add to new group", [this, name]
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

		AddBookToGroup(*transaction, m_impl->bookId, id);

		transaction->Commit();

		return [] {};
	} });
}

void GroupsModelController::AddTo(const QString & id)
{
	m_impl->executor({ "Add to group", [this, id = id.toLongLong()]
	{
		const auto transaction = m_impl->db.CreateTransaction();
		AddBookToGroup(*transaction, m_impl->bookId, id);
		transaction->Commit();
		return [] {};
	} });
}

void GroupsModelController::RemoveFrom(const QString & id)
{
	m_impl->executor({ "Remove from group", [this, id = id.toInt()]
	{
		std::string queryText = "delete from Groups_List_User where BookID = " + std::to_string(m_impl->bookId);
		if (id > 0)
			queryText.append(" and GroupID = ").append(std::to_string(id));

		const auto transaction = m_impl->db.CreateTransaction();
		transaction->CreateCommand(queryText)->Execute();
		transaction->Commit();

		return [] { };
	} });
}

}
