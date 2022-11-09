#pragma once

#include <QMetaType>

#include "ModelObserver.h"

namespace HomeCompa::Flibrary {

class NavigationModelObserver : virtual public ModelObserver
{
};

}

Q_DECLARE_METATYPE(HomeCompa::Flibrary::NavigationModelObserver *);
