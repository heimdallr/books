#pragma once

#include <QMetaType>

#include "IModelObserver.h"

namespace HomeCompa::Flibrary {

class NavigationModelObserver : virtual public IModelObserver
{
};

}

Q_DECLARE_METATYPE(HomeCompa::Flibrary::NavigationModelObserver *);
