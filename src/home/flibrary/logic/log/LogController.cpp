#include "LogController.h"

#include <QAbstractItemModel>
#include <plog/Severity.h>

#include "model/LogModel.h"

using namespace HomeCompa::Flibrary;

namespace {

static_assert(plog::Severity::none == 0);
static_assert(plog::Severity::fatal == 1);
static_assert(plog::Severity::error == 2);
static_assert(plog::Severity::warning == 3);
static_assert(plog::Severity::info == 4);
static_assert(plog::Severity::debug == 5);
static_assert(plog::Severity::verbose == 6);

constexpr auto NONE = QT_TRANSLATE_NOOP("Logging", "NONE");
constexpr auto FATAL = QT_TRANSLATE_NOOP("Logging", "FATAL");
constexpr auto ERROR = QT_TRANSLATE_NOOP("Logging", "ERROR");
constexpr auto WARN = QT_TRANSLATE_NOOP("Logging", "WARN");
constexpr auto INFO = QT_TRANSLATE_NOOP("Logging", "INFO");
constexpr auto DEBUG = QT_TRANSLATE_NOOP("Logging", "DEBUG");
constexpr auto VERB = QT_TRANSLATE_NOOP("Logging", "VERB");

constexpr const char * SEVERITIES[]
{
	NONE,
	FATAL,
	ERROR,
	WARN,
	INFO,
	DEBUG,
	VERB,
};

}

struct LogController::Impl
{
	std::unique_ptr<QAbstractItemModel> model { CreateLogModel() };
};

LogController::LogController() = default;
LogController::~LogController() = default;

QAbstractItemModel * LogController::GetModel() const noexcept
{
	return m_impl->model.get();
}

void LogController::Clear()
{
	m_impl->model->setData({}, {}, LogModelRole::Clear);
}

std::vector<const char *> LogController::GetSeverities() const
{
	return { std::cbegin(SEVERITIES), std::cend(SEVERITIES) };
}

int LogController::GetSeverity() const
{
	return m_impl->model->data({}, LogModelRole::Severity).toInt();
}

void LogController::SetSeverity(const int value)
{
	m_impl->model->setData({}, value, LogModelRole::Severity);
}
