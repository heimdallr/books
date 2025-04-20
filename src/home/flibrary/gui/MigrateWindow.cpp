#include "MigrateWindow.h"

#include <QCoreApplication>
#include <QTimer>

#include "interface/logic/IDatabaseMigrator.h"

#include "GuiUtil/GeometryRestorable.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

struct MigrateWindow::Impl
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, IDatabaseMigrator::IObserver
{
	PropagateConstPtr<IDatabaseMigrator, std::shared_ptr> migrator;

	Impl(QWidget& self, std::shared_ptr<ISettings> settings, std::shared_ptr<IDatabaseMigrator> migrator)
		: GeometryRestorable(*this, std::move(settings), "MigrateWindow")
		, GeometryRestorableObserver(self)
		, migrator { std::move(migrator) }
	{
		this->migrator->RegisterObserver(this);
		Init();
	}

	~Impl() override
	{
		migrator->UnregisterObserver(this);
	}

private: // IDatabaseMigrator::IObserver
	void OnMigrationFinished() override
	{
		QTimer::singleShot(0, [] { QCoreApplication::exit(); });
	}

	NON_COPY_MOVABLE(Impl)
};

MigrateWindow::MigrateWindow(std::shared_ptr<ISettings> settings, std::shared_ptr<IDatabaseMigrator> migrator, const std::shared_ptr<const ILogController>& logController, QWidget* parent)
	: QListView(parent)
	, m_impl(*this, std::move(settings), std::move(migrator))
{
	PLOGV << "MigrateWindow created";
	setWindowTitle(tr("The database migration process is in progress..."));
	setMinimumSize(QSize { 1024, 720 });
	setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
	QListView::setModel(logController->GetModel());
}

MigrateWindow::~MigrateWindow()
{
	PLOGV << "MigrateWindow destroyed";
}

void MigrateWindow::Show()
{
	show();
	m_impl->migrator->Migrate();
}
