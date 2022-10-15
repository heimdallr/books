#pragma once

#include <QMetaType>

#include "ModelObserver.h"

namespace HomeCompa::Flibrary {

class AuthorsModelObserver
	: public ModelObserver
{
};

}

Q_DECLARE_METATYPE(HomeCompa::Flibrary::AuthorsModelObserver *);
