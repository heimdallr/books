#include <QAbstractItemModel>
#include <QQmlEngine>

#include "models/LogModel.h"
#include "LogController.h"

namespace HomeCompa::Flibrary {

LogController::LogController(QObject * parent)
	: QObject(parent)
	, m_model(CreateLogModel(this))
{
	QQmlEngine::setObjectOwnership(m_model, QQmlEngine::CppOwnership);
}

QAbstractItemModel * LogController::GetModel() const
{
	return m_model;
}

bool LogController::IsLogMode() const noexcept
{
	return m_logMode;
}

void LogController::SetLogMode(const bool value)
{
	m_logMode = value;
	emit LogModeChanged();
}

}
