#include "ScrollBarController.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>

using namespace HomeCompa::Flibrary;

ScrollBarController::ScrollBarController(QObject* parent)
	: QObject(parent)
	, m_timerV { CreateTimer(&ScrollBarController::OnTimeoutV) }
	, m_timerH { CreateTimer(&ScrollBarController::OnTimeoutH) }
{
}

void ScrollBarController::SetScrollArea(QAbstractScrollArea* area)
{
	m_area = area;
}

bool ScrollBarController::eventFilter(QObject* /*obj*/, QEvent* event)
{
	if (event->type() == QEvent::Enter)
	{
		m_timerV->stop();
		m_timerH->stop();
		return false;
	}

	if (event->type() == QEvent::Leave)
	{
		m_timerV->start();
		m_timerH->start();
		return false;
	}

	if (event->type() == QEvent::MouseMove)
	{
		OnTimeoutV();
		OnTimeoutH();
	}

	return false;
}

QTimer* ScrollBarController::CreateTimer(void (ScrollBarController::*f)() const)
{
	auto* timer = new QTimer(this);
	timer->setSingleShot(false);
	timer->setInterval(std::chrono::milliseconds(200));
	connect(timer, &QTimer::timeout, this, f);
	return timer;
}

void ScrollBarController::OnTimeoutV() const
{
	if (!m_area)
		return;

	auto& area = *m_area;
	const auto& viewport = *area.viewport();
	const auto threshold = QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	const auto pos = QCursor::pos();

	auto rect = viewport.geometry();
	rect.setX(rect.width() - threshold - (area.verticalScrollBar()->isVisible() ? 0 : threshold));
	rect.setWidth(5 * threshold / 2);
	const auto topLeft = viewport.mapToGlobal(rect.topLeft());
	const auto bottomRight = viewport.mapToGlobal(rect.bottomRight());
	if (QRect(topLeft, bottomRight).contains(pos))
		return area.setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	area.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_timerV->stop();
}

void ScrollBarController::OnTimeoutH() const
{
	if (!m_area)
		return;

	auto& area = *m_area;
	const auto& viewport = *area.viewport();
	const auto threshold = QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	const auto pos = QCursor::pos();

	auto rect = viewport.geometry();
	rect.setY(rect.height() - threshold - (area.horizontalScrollBar()->isVisible() ? 0 : threshold));
	rect.setHeight(5 * threshold / 2);
	const auto topLeft = viewport.mapToGlobal(rect.topLeft());
	const auto bottomRight = viewport.mapToGlobal(rect.bottomRight());
	if (QRect(topLeft, bottomRight).contains(pos))
		return area.setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	area.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_timerH->stop();
}
