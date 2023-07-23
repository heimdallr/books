#pragma once

#include <QObject>

#include "UtilLib.h"

namespace HomeCompa {

class UTIL_API Measure final
	: public QObject
{
	Q_OBJECT

public:
	explicit Measure(QObject * parent = nullptr);

	Q_INVOKABLE static QString GetSize(qulonglong size);
};

}
