#include "ClickableLabel.h"

#include <QMouseEvent>

using namespace HomeCompa::Flibrary;

ClickableLabel::ClickableLabel(QWidget * parent)
	: QLabel(parent)
{
}

void ClickableLabel::mousePressEvent(QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton)
		emit clicked(event->pos());
}
