#pragma once

#include <map>
#include <set>

#include "fnd/algorithm.h"

#include "util/StrUtil.h"

namespace HomeCompa
{

//AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;RATE;KEYWORDS;YEAR;SOURCELIB;
#define BOOK_BUF_FIELD_ITEMS_XMACRO \
	BOOK_BUF_FIELD_ITEM(AUTHOR)     \
	BOOK_BUF_FIELD_ITEM(GENRE)      \
	BOOK_BUF_FIELD_ITEM(TITLE)      \
	BOOK_BUF_FIELD_ITEM(SERIES)     \
	BOOK_BUF_FIELD_ITEM(SERNO)      \
	BOOK_BUF_FIELD_ITEM(FILE)       \
	BOOK_BUF_FIELD_ITEM(SIZE)       \
	BOOK_BUF_FIELD_ITEM(LIBID)      \
	BOOK_BUF_FIELD_ITEM(DEL)        \
	BOOK_BUF_FIELD_ITEM(EXT)        \
	BOOK_BUF_FIELD_ITEM(DATE)       \
	BOOK_BUF_FIELD_ITEM(FOLDER)     \
	BOOK_BUF_FIELD_ITEM(LANG)       \
	BOOK_BUF_FIELD_ITEM(LIBRATE)    \
	BOOK_BUF_FIELD_ITEM(KEYWORDS)   \
	BOOK_BUF_FIELD_ITEM(YEAR)       \
	BOOK_BUF_FIELD_ITEM(SOURCELIB)

struct Book
{
	size_t id { 0 };
#define BOOK_BUF_FIELD_ITEM(NAME) QStringView NAME;
	BOOK_BUF_FIELD_ITEMS_XMACRO
#undef BOOK_BUF_FIELD_ITEM
	QString fileName;
	QString folder;
	size_t  folderId { 0 };
	size_t  updateId { 0 };
};

struct Genre
{
	QString code;
	QString parentCore;
	QString name;
	size_t  parentId { 0 };
	QString dbCode;

	size_t childrenCount { 0 };
	bool   newGenre { true };

	explicit Genre(QString dbCode_)
		: dbCode { std::move(dbCode_) }
	{
	}

	Genre(QString code_, QString parentCode_, QString name_, const size_t parentId_ = 0)
		: code { std::move(code_) }
		, parentCore { std::move(parentCode_) }
		, name { std::move(name_) }
		, parentId { parentId_ }
	{
	}
};

struct Update
{
	size_t                          id { 0 };
	int                             title { 0 };
	size_t                          parentId { 0 };
	std::unordered_map<int, Update> children {};
};

inline std::ostream& operator<<(std::ostream& stream, const Book& book)
{
	return stream << book.FOLDER.toString().toStdString() << ", " << book.LIBID.toString().toStdString() << ": " << book.id << ", " << book.TITLE.toString().toStdString();
}

inline std::ostream& operator<<(std::ostream& stream, const Genre& genre)
{
	return stream << genre.dbCode.toStdString() << ", " << genre.code.toStdString() << ": " << genre.name.toStdString();
}

} // namespace HomeCompa
