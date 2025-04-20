#pragma once

#include <QListView>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDatabaseMigrator.h"
#include "interface/logic/ILogController.h"
#include "interface/ui/IMigrateWindow.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class MigrateWindow final
	: public QListView
	, virtual public IMigrateWindow
{
	Q_OBJECT
	NON_COPY_MOVABLE(MigrateWindow)

public:
	MigrateWindow(std::shared_ptr<ISettings> settings, std::shared_ptr<IDatabaseMigrator> migrator, const std::shared_ptr<const ILogController>& logController, QWidget* parent = nullptr);
	~MigrateWindow() override;

private: // IMigrateWindow
	void Show() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
