#pragma once

#include <QObject>

namespace HomeCompa::Flibrary {

class Measure
	: public QObject
{
	Q_OBJECT

public:
	explicit Measure(QObject * parent = nullptr);

	Q_INVOKABLE static QString GetSize(qulonglong size);
};

}
