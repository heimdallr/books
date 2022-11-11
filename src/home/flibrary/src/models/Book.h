#pragma once

#include <QString>

#include "BookDecl.h"

namespace HomeCompa::Flibrary {

struct Book
{
	long long int Id { -1 };
	size_t ParentId { static_cast<size_t>(-1) };
	QString Title;
	int SeqNumber;
	QString UpdateDate;
	int LibRate;
	QString Lang;
	QString Folder;
	QString FileName;
	size_t Size;
	bool IsDeleted { true };
	QString Author;
	QString AuthorFull;
	QString GenreAlias;
	QString SeriesTitle;
	bool Checked { false };
	bool IsDictionary { false };
	bool Expanded { false };
	int TreeLevel { 0 };
};

}
