#pragma once

#include <QObject>
#include <qnamespace.h>

namespace HomeCompa::Flibrary {

struct RoleBase : QObject
{
	enum ValueBase
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
	Q_ENUM(ValueBase)
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
