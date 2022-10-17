#pragma once

#include <qnamespace.h>
#include <QObject>

namespace HomeCompa::Flibrary {

struct RoleBase : public QObject
{
	Q_OBJECT
public:
	enum ValueBase
	{
		FakeRoleFirst = Qt::UserRole + 1,

		// чтение локальное
		Id,
		Title,

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
