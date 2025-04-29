#include "ui_QueryWindow.h"

#include "QueryWindow.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "GuiUtil/GeometryRestorable.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

class QueryWindow::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
public:
	Impl(QMainWindow* self, std::shared_ptr<ISettings> settings, std::shared_ptr<IDatabaseUser> databaseUser)
		: GeometryRestorable(*this, std::move(settings), "QueryWindow")
		, GeometryRestorableObserver(*self)
		, m_databaseUser { std::move(databaseUser) }
	{
		m_ui.setupUi(self);

		connect(m_ui.actionStartTransaction, &QAction::triggered, [this] { StartTransaction(); });
		connect(m_ui.actionCommit, &QAction::triggered, [this] { Commit(); });
		connect(m_ui.actionRollback, &QAction::triggered, [this] { Rollback(); });
		connect(m_ui.actionExecute, &QAction::triggered, [this] { Execute(); });
		connect(m_ui.actionExit, &QAction::triggered, [self] { self->hide(); });

		self->addAction(m_ui.actionStartTransaction);
		self->addAction(m_ui.actionCommit);
		self->addAction(m_ui.actionRollback);
		self->addAction(m_ui.actionExecute);
		self->addAction(m_ui.actionExit);

		Init();
	}

private:
	void StartTransaction()
	{
		m_transaction = m_databaseUser->Database()->CreateTransaction();
		SetEnabled();
	}

	void Commit()
	{
		m_transaction->Commit();
		m_transaction.reset();
		SetEnabled();
	}

	void Rollback()
	{
		m_transaction->Rollback();
		m_transaction.reset();
		SetEnabled();
	}

	void Execute() const
	{
		m_transaction ? ExecuteCommand() : ExecuteQuery();
	}

	void ExecuteCommand() const
	{
		try
		{
			m_transaction->CreateCommand(m_ui.text->toPlainText().toStdString())->Execute();
		}
		catch (const std::exception& ex)
		{
			PLOGE << "Error: " << ex.what();
		}
	}

	void ExecuteQuery() const
	{
		m_databaseUser->Execute({ "Execute query",
		                          [this]
		                          {
									  ExecuteQueryImpl();
									  return [](size_t) {};
								  } });
	}

	void ExecuteQueryImpl() const
	{
		const auto db = m_databaseUser->Database();
		const auto query = db->CreateQuery(m_ui.text->toPlainText().toStdString());
		query->Execute();
		{
			QStringList names;
			for (size_t i = 0, sz = query->ColumnCount(); i < sz; ++i)
				names << QString::fromStdString(query->ColumnName(i));
			PLOGI << names.join(" | ");
		}

		for (; !query->Eof(); query->Next())
		{
			QStringList values;
			for (size_t i = 0, sz = query->ColumnCount(); i < sz; ++i)
				values << query->Get<const char*>(i);
			PLOGI << values.join(" | ");
		}
	}

	void SetEnabled() const
	{
		m_ui.actionStartTransaction->setEnabled(!m_transaction);
		m_ui.actionCommit->setEnabled(!!m_transaction);
		m_ui.actionRollback->setEnabled(!!m_transaction);
	}

private:
	Ui::QueryWindow m_ui {};
	std::shared_ptr<IDatabaseUser> m_databaseUser;
	std::shared_ptr<DB::ITransaction> m_transaction;
};

QueryWindow::QueryWindow(std::shared_ptr<ISettings> settings, std::shared_ptr<IDatabaseUser> databaseUser, QWidget* parent)
	: QMainWindow(parent)
	, m_impl(this, std::move(settings), std::move(databaseUser))
{
}

QueryWindow::~QueryWindow() = default;
