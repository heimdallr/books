#pragma once

#include <qlogging.h>

class QMessageLogContext;
class QString;

class QtLogHandler
{
public:
	QtLogHandler();
	~QtLogHandler();

private:
	static void HandleStatic(QtMsgType type, const QMessageLogContext& ctx, const QString& message);
	void        Handle(QtMsgType type, const QMessageLogContext& ctx, const QString& message) const;

private:
	QtMessageHandler     m_qtLogHandlerPrev;
	static QtLogHandler* s_qtLogHandler;
};
