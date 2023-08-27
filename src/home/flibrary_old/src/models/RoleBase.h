#pragma once

#include <qnamespace.h>
#include <QObject>
#include <QVariant>

namespace HomeCompa::Flibrary {

struct RoleBase : QObject
{
	Q_OBJECT
public:
	enum ValueBase
	{
		FakeRoleFirst = Qt::UserRole + 1,

		// чтение локальное
		Id,
		Title,

		// запись локальная
		Click,
		DoubleClick,
		KeyPressed,

		// чтение глобальное
		Count,

		// запись глобальная
		ResetBegin,
		ResetEnd,
		ObserverRegister,
		ObserverUnregister,
		Find,
		Filter,

		// чтение через глобальную запись
		TranslateIndexFromGlobal,
		CheckIndexVisible,
		IncreaseLocalIndex,
		FindItem,

		FakeRoleLast
	};
	Q_ENUM(ValueBase)
};

struct TranslateIndexFromGlobalRequest
{
	int * index;
};

struct CheckIndexVisibleRequest
{
	int * visibleIndex;
};

struct IncreaseLocalIndexRequest
{
	int index;
	int * incrementedIndex;
};

struct FindItemRequest
{
	QVariant itemId;
	int * itemIndex;
};

}

Q_DECLARE_METATYPE(HomeCompa::Flibrary::TranslateIndexFromGlobalRequest);
Q_DECLARE_METATYPE(HomeCompa::Flibrary::CheckIndexVisibleRequest);
Q_DECLARE_METATYPE(HomeCompa::Flibrary::IncreaseLocalIndexRequest);
Q_DECLARE_METATYPE(HomeCompa::Flibrary::FindItemRequest);