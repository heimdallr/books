#include "util.h"

#include <QWidget>

namespace HomeCompa::Util
{

QRect GetGlobalGeometry(const QWidget& widget)
{
	const auto geometry = widget.geometry();
	const auto* parent = widget.parentWidget();
	const auto topLeft = parent->mapToGlobal(geometry.topLeft());
	const auto bottomRight = parent->mapToGlobal(geometry.bottomRight());
	QRect rect { topLeft, bottomRight };
	return rect;
}

}
