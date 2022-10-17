#pragma once

#include <QString>

#include "AuthorDecl.h"

namespace HomeCompa::Flibrary {

struct Author
{
	long long int Id { 0 };
	QString Title;
};

}
