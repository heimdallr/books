#pragma once

#include <QObject>

#include "fnd/NonCopyMovable.h"

#include "util_export.h"

namespace HomeCompa::Util {

class UTIL_EXPORT ObjectsConnector
	: public QObject
{
	NON_COPY_MOVABLE(ObjectsConnector)
	Q_OBJECT

public:
	explicit ObjectsConnector(QObject * parent = nullptr);
	~ObjectsConnector() override;

public:
	Q_INVOKABLE static void registerEmitter(QString ID, QObject * sender, const QString & signal, bool queued = false);
	Q_INVOKABLE static void registerReceiver(QString ID, QObject * receiver, const QString & slot, bool queued = false);
	Q_INVOKABLE static void unregisterEmitter(const QString & ID, QObject * sender, const QString & signal);
	Q_INVOKABLE static void unregisterReceiver(const QString & ID, QObject * receiver, const QString & slot);
};

}
