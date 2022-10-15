#pragma once

#include <qnamespace.h>

namespace HomeCompa::Flibrary {

struct BaseRole
{
	enum BaseValue
	{
		ResetBegin = Qt::UserRole + 1,
		ResetEnd,
		ObserverRegister,
		ObserverUnregister,
		Find,
		Filter,
		Last
	};
};

}