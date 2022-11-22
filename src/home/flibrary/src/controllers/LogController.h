#pragma once

#include <QObject>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class SettingsProvider;
class LogController final
	: public QObject
{
	NON_COPY_MOVABLE(LogController)
	Q_OBJECT
	Q_PROPERTY(bool logMode READ IsLogMode WRITE SetLogMode NOTIFY LogModeChanged)

signals:
	void LogModeChanged() const;

public:
	Q_INVOKABLE void Clear();
	Q_INVOKABLE QAbstractItemModel * GetLogModel() const;
	Q_INVOKABLE QAbstractItemModel * GetSeverityModel() const;
	Q_INVOKABLE static void Error(const QString & message);
	Q_INVOKABLE static void Warning(const QString & message);
	Q_INVOKABLE static void Info(const QString & message);
	Q_INVOKABLE static void Debug(const QString & message);
	Q_INVOKABLE static void Verbose(const QString & message);

public:
	explicit LogController(SettingsProvider & settingsProvider, QObject * parent = nullptr);
	~LogController() override;

public:
	void OnKeyPressed(int key, int modifiers);

private: // property getters
	bool IsLogMode() const noexcept;

private slots: // property setters
	void SetLogMode(bool value);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
