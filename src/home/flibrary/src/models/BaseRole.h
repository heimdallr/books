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
		Click,
		TranslateIndexFromGlobal,
		CheckIndexVisible,
		Last
	};
};

struct TranslateIndexFromGlobalRequest
{
	int globalIndex;
	int * localIndex;
};

struct CheckIndexVisibleRequest
{
	int * visibleIndex;
};

}

Q_DECLARE_METATYPE(HomeCompa::Flibrary::TranslateIndexFromGlobalRequest);
Q_DECLARE_METATYPE(HomeCompa::Flibrary::CheckIndexVisibleRequest);
