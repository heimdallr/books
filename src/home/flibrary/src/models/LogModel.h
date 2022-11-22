#pragma once

#include "controllers/SettingsProvider.h"

class QAbstractItemModel;
class QColor;
class QObject;

namespace HomeCompa::Flibrary {

struct LogModelRole
{
	enum Value
	{
		Message = Qt::UserRole + 1,
		Color,
		Clear,
	};
};

class LogModelController
	: virtual public SettingsProvider
{
public:
	virtual const QColor & GetColor(plog::Severity severity) const = 0;
};

QAbstractItemModel * CreateLogModel(LogModelController & controller, QObject * parent = nullptr);

}
