#pragma once

#include "controllers/ISettingsProvider.h"

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

class ILogModelController
	: virtual public ISettingsProvider
{
public:
	virtual const QColor & GetColor(plog::Severity severity) const = 0;
};

QAbstractItemModel * CreateLogModel(ILogModelController & controller, QObject * parent = nullptr);

}
