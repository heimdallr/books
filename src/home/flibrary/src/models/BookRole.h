#pragma once

#include <QObject>

#include "RoleBase.h"

namespace HomeCompa::Flibrary {

#define BOOK_ROLE_ITEMS_XMACRO        \
		BOOK_ROLE_ITEM(SeqNumber)     \
		BOOK_ROLE_ITEM(UpdateDate)    \
		BOOK_ROLE_ITEM(LibRate)       \
		BOOK_ROLE_ITEM(Lang)          \
		BOOK_ROLE_ITEM(Folder)        \
		BOOK_ROLE_ITEM(FileName)      \
		BOOK_ROLE_ITEM(IsDeleted)     \
		BOOK_ROLE_ITEM(Author)        \
		BOOK_ROLE_ITEM(GenreAlias)    \
		BOOK_ROLE_ITEM(SeriesTitle)   \
		BOOK_ROLE_ITEM(Checked)       \
		BOOK_ROLE_ITEM(IsDictionary)  \
		BOOK_ROLE_ITEM(Expanded)      \
		BOOK_ROLE_ITEM(TreeLevel)     \

struct BookRole
	: RoleBase
{
	Q_OBJECT
public:
	enum Value
	{
		FakeBookRoleFirst = FakeRoleLast + 1,
#define	BOOK_ROLE_ITEM(NAME) NAME,
		BOOK_ROLE_ITEMS_XMACRO
#undef	BOOK_ROLE_ITEM
		ShowDeleted,
		Language,
		Languages,
		FakeBookRoleLast
	};

	Q_ENUM(Value)
};
