#pragma once

#include "ModelObserver.h"

#include <QMetaType>

namespace HomeCompa::Flibrary {

class AuthorsModelObserver
	: virtual public ModelObserver
{
};

}

Q_DECLARE_METATYPE(HomeCompa::Flibrary::AuthorsModelObserver *);
