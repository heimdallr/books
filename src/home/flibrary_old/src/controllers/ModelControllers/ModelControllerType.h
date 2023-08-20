#pragma once

#include <QObject>

namespace HomeCompa::Flibrary {

class ModelControllerTypeClass
	: public QObject
{
	Q_OBJECT
public:
	enum class Type
	{
		Navigation,
		Books,
	};
	Q_ENUM(Type)

private:
	ModelControllerTypeClass();
};

using ModelControllerType = ModelControllerTypeClass::Type;

}
