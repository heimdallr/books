#pragma once

#include <qnamespace.h>

namespace HomeCompa::Flibrary {

struct BaseRole
{
	enum BaseValue
	{
		FakeRoleFirst = Qt::UserRole + 1,

		// запись глобальная
		ResetBegin,
		ResetEnd,
		ObserverRegister,
		ObserverUnregister,
		Find,
		Filter,
		TranslateIndexFromGlobal,
		CheckIndexVisible,

		// локальная
		Click,

		FakeRoleLast
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
