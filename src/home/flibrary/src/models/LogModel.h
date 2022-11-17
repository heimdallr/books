#pragma once

#include "controllers/SettingsProvider.h"

class QAbstractItemModel;
class QColor;
class QObject;

namespace HomeCompa::Flibrary {

class LogModelController
	: virtual public SettingsProvider
{
public:
	virtual const QColor & GetColor(plog::Severity severity) const = 0;
};

QAbstractItemModel * CreateLogModel(LogModelController & controller, QObject * parent = nullptr);

}
