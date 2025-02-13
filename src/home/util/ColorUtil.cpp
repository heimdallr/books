#include "ColorUtil.h"

namespace HomeCompa::Util
{

QString ToString(const QColor& color)
{
	return QString("#%1%2%3").arg(color.red(), 2, 16, QChar { '0' }).arg(color.green(), 2, 16, QChar { '0' }).arg(color.blue(), 2, 16, QChar { '0' });
}

QString ToString(const QPalette& palette, const QPalette::ColorRole role)
{
	return ToString(palette.color(role));
}

}
