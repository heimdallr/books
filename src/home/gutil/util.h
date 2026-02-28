#pragma once

#include <QRect>

#include "export/gutil.h"

namespace HomeCompa
{

class ISettings;

}

class QHeaderView;
class QMenu;
class QTreeView;
class QWidget;

namespace HomeCompa::Util
{

GUTIL_EXPORT QRect  GetGlobalGeometry(const QWidget& widget);
GUTIL_EXPORT void   SaveHeaderSectionWidth(const QHeaderView& header, ISettings& settings, const QString& key);
GUTIL_EXPORT void   LoadHeaderSectionWidth(QHeaderView& header, const ISettings& settings, const QString& key);
GUTIL_EXPORT QMenu& FillTreeContextMenu(QTreeView& view, QMenu& menu);

}
