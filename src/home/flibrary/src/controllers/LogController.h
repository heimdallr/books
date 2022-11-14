#pragma once

#include <QObject>

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class LogController
	: public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool logMode READ IsLogMode WRITE SetLogMode NOTIFY LogModeChanged)

signals:
	void LogModeChanged() const;

public:
	Q_INVOKABLE QAbstractItemModel * GetModel() const;

public:
	explicit LogController(QObject * parent = nullptr);

private: // property getters
	bool IsLogMode() const noexcept;

private: // property setters
	void SetLogMode(bool value);

private:
	bool m_logMode { false };
	QAbstractItemModel * m_model;
};

}
