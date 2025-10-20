#include "QtLoHandler.h"

#include <cassert>

#include <QFileInfo>

#include "log.h"

namespace
{

QString FileName(const QMessageLogContext& ctx)
{
	return ctx.file ? QFileInfo(ctx.file).fileName() : "undefined";
}

constexpr const char* IGNORED[] {
	"DirectWrite: CreateFontFaceFromHDC() failed",
	"QWindowsWindow::setGeometry: Unable to set geometry",
	"QApplication::regClass: Registering window class",
};

}

QtLogHandler* QtLogHandler::s_qtLogHandler = nullptr;

QtLogHandler::QtLogHandler()
	: m_qtLogHandlerPrev(qInstallMessageHandler(&QtLogHandler::HandleStatic))
{
	assert(!s_qtLogHandler && "once time only");
	s_qtLogHandler = this;
}

QtLogHandler::~QtLogHandler()
{
	assert(s_qtLogHandler);
	qInstallMessageHandler(*m_qtLogHandlerPrev);
	s_qtLogHandler = nullptr;
}

void QtLogHandler::HandleStatic(const QtMsgType type, const QMessageLogContext& ctx, const QString& message)
{
	assert(s_qtLogHandler);
	s_qtLogHandler->Handle(type, ctx, message);
	s_qtLogHandler->m_qtLogHandlerPrev(type, ctx, message);
}

void QtLogHandler::Handle(const QtMsgType type, const QMessageLogContext& ctx, const QString& message) const
{
	if (std::ranges::any_of(IGNORED, [&](const auto* item) {
			return message.startsWith(item);
		}))
		return;

	const auto context = QString("[%1@%2] ").arg(FileName(ctx)).arg(ctx.line);

	switch (type)
	{
		case QtDebugMsg:
			PLOGD << context << message;
			break;
		case QtInfoMsg:
			PLOGI << context << message;
			break;
		case QtWarningMsg:
			PLOGW << context << message;
			break;
		case QtCriticalMsg:
			PLOGE << context << message;
			break;
		case QtFatalMsg:
			PLOGF << context << message;
			break;
		default: // NOLINT(clang-diagnostic-covered-switch-default)
			assert(false);
	}
}
