#pragma once

#include <QString>

#include "BookDecl.h"

namespace HomeCompa::Flibrary {

struct Book
{
	long long int Id { 0 };
	QString Title;
};

}
