#pragma once

#include <QMainWindow>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDatabaseUser.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class QueryWindow : public QMainWindow
{
	Q_OBJECT
	NON_COPY_MOVABLE(QueryWindow)

public:
	QueryWindow(std::shared_ptr<ISettings> settings, std::shared_ptr<IDatabaseUser> databaseUser, QWidget* parent = nullptr);
	~QueryWindow() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
