#include <QAbstractItemModel>
#include <QColor>
#include <QQmlEngine>

#include <plog/Log.h>
#include <plog/Severity.h>

#include "constants/ObjectConnectorConstant.h"

#include "models/LogModel.h"
#include "models/SimpleModel.h"

#include "util/Settings.h"

#include "LogController.h"

#include "Settings/UiSettings_keys.h"
#include "Settings/UiSettings_values.h"

namespace HomeCompa::Flibrary {

namespace {

static_assert(plog::Severity::none == 0);
static_assert(plog::Severity::fatal == 1);
static_assert(plog::Severity::error == 2);
static_assert(plog::Severity::warning == 3);
static_assert(plog::Severity::info == 4);
static_assert(plog::Severity::debug == 5);
static_assert(plog::Severity::verbose == 6);

SimpleModeItems CreateSeverityItems(int levelsCount)
{
	SimpleModeItems result;
	for (int i = 1; i < levelsCount; ++i)
		result.emplace_back(QString::number(i), severityToString(static_cast<plog::Severity>(i)));
	return result;
}

}

struct LogController::Impl final
	: virtual private LogModelController
{
private:
	SettingsProvider & m_settingsProvider;

public:
	bool logMode { false };
	std::vector<QColor> colors
	{
		QColor(m_settingsProvider.GetUiSettings().Get(Constant::UiSettings_ns::colorLogDefault, Constant::UiSettings_ns::colorLogDefault_default).toString()),
		QColor(m_settingsProvider.GetUiSettings().Get(Constant::UiSettings_ns::colorLogFatal  , Constant::UiSettings_ns::colorLogFatal_default  ).toString()),
		QColor(m_settingsProvider.GetUiSettings().Get(Constant::UiSettings_ns::colorLogError  , Constant::UiSettings_ns::colorLogError_default  ).toString()),
		QColor(m_settingsProvider.GetUiSettings().Get(Constant::UiSettings_ns::colorLogWarning, Constant::UiSettings_ns::colorLogWarning_default).toString()),
		QColor(m_settingsProvider.GetUiSettings().Get(Constant::UiSettings_ns::colorLogInfo   , Constant::UiSettings_ns::colorLogInfo_default   ).toString()),
		QColor(m_settingsProvider.GetUiSettings().Get(Constant::UiSettings_ns::colorLogDebug  , Constant::UiSettings_ns::colorLogDebug_default  ).toString()),
		QColor(m_settingsProvider.GetUiSettings().Get(Constant::UiSettings_ns::colorLogVerbose, Constant::UiSettings_ns::colorLogVerbose_default).toString()),
	};
	QAbstractItemModel * modelLog;
	QAbstractItemModel * modelSeverity;

	explicit Impl(LogController & self, SettingsProvider & settingsProvider)
		: m_settingsProvider(settingsProvider)
		, modelLog(CreateLogModel(*this, &self))
		, modelSeverity(CreateSimpleModel(CreateSeverityItems(static_cast<int>(std::size(colors))), &self))
	{
		QQmlEngine::setObjectOwnership(modelLog, QQmlEngine::CppOwnership);
		QQmlEngine::setObjectOwnership(modelSeverity, QQmlEngine::CppOwnership);
	}

private: // SettingsProvider
	Settings & GetSettings() override
	{
		return m_settingsProvider.GetSettings();
	}

	Settings & GetUiSettings() override
	{
		return m_settingsProvider.GetUiSettings();
	}

private: // LogModelController
	const QColor & GetColor(const plog::Severity severity) const override
	{
		const auto index = static_cast<int>(severity);
		return index < 1 || index >= std::size(colors) ? assert(false && "unexpected severity"), colors[0] : colors[index];
	}
};

LogController::LogController(SettingsProvider & settingsProvider, QObject * parent)
	: QObject(parent)
	, m_impl(*this, settingsProvider)
{
	Util::ObjectsConnector::registerReceiver(ObjConn::SHOW_LOG, this, SLOT(SetLogMode(bool)));
}

LogController::~LogController() = default;

void LogController::OnKeyPressed(int key, int modifiers)
{
	if (key != Qt::Key_Escape || modifiers != Qt::NoModifier)
		return;

	m_impl->logMode = !m_impl->logMode;
	emit LogModeChanged();
}

QAbstractItemModel * LogController::GetLogModel() const
{
	return m_impl->modelLog;
}

QAbstractItemModel * LogController::GetSeverityModel() const
{
	return m_impl->modelSeverity;
}

void LogController::Error(const QString & message)
{
	PLOGE << message;
}

void LogController::Warning(const QString & message)
{
	PLOGW << message;
}

void LogController::Info(const QString & message)
{
	PLOGI << message;
}

void LogController::Debug(const QString & message)
{
	PLOGD << message;
}

void LogController::Verbose(const QString & message)
{
	PLOGV << message;
}


bool LogController::IsLogMode() const noexcept
{
	return m_impl->logMode;
}

void LogController::SetLogMode(const bool value)
{
	m_impl->logMode = value;
	emit LogModeChanged();
}

}
