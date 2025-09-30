#include "ClickableLabel.h"

#include <QMouseEvent>
#include <QTimer>

using namespace HomeCompa::Flibrary;

struct ClickableLabel::Impl
{
	QTimer timer;
	QPoint pos;

	Impl()
	{
		timer.setSingleShot(true);
		timer.setInterval(std::chrono::milliseconds(300));
	}
};

ClickableLabel::ClickableLabel(QWidget* parent)
	: QLabel(parent)
{
	connect(&m_impl->timer, &QTimer::timeout, [this] {
		emit clicked(m_impl->pos);
	});
}

ClickableLabel::~ClickableLabel() = default;

void ClickableLabel::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		if (m_impl->timer.isActive())
		{
			m_impl->timer.stop();
			return emit doubleClicked(event->pos());
		}

		m_impl->pos = event->pos();
		m_impl->timer.start();
	}
}

void ClickableLabel::enterEvent(QEnterEvent* event)
{
	emit mouseEnter(event);
}

void ClickableLabel::leaveEvent(QEvent* event)
{
	emit mouseLeave(event);
}
