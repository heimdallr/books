#pragma once

#include <QString>

#include "BookDecl.h"

namespace HomeCompa::Flibrary {

struct Book
{
	long long int Id { 0 };
	QString Title;
	int SeqNumber;
	QString UpdateDate;
	int LibRate;
	QString Lang;
	QString Folder;
	QString FileName;
	bool IsDeleted;
	QString Author;
	QString GenreAlias;
	QString SeriesTitle;
	bool Checked { false };
};

}
