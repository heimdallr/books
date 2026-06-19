#include "RangeSlider.h"

#include <QApplication>
#include <QPainter>
#include <QStyleOptionSlider>

#include "fnd/ScopedCall.h"
#include "fnd/algorithm.h"

using namespace HomeCompa::Flibrary;

RangeSlider::RangeSlider(QWidget* parent)
	: QSlider(parent)
	, m_low { minimum() }
	, m_high { maximum() }
{
}

int RangeSlider::low() const noexcept
{
	return m_low;
}

void RangeSlider::setLow(const int lowLimit) noexcept
{
	m_low = lowLimit;
	update();
}

int RangeSlider::high() const noexcept
{
	return m_high;
}

void RangeSlider::setHigh(const int highLimit) noexcept
{
	m_high = highLimit;
	update();
}

void RangeSlider::paintEvent(QPaintEvent*)
{
	QPainter           painter(this);
	QStyleOptionSlider opt;

	// Draw groove
	initStyleOption(&opt);
	opt.sliderValue    = 0;
	opt.sliderPosition = 0;
	opt.subControls    = QStyle::SC_SliderGroove | (tickPosition() != NoTicks ? QStyle::SC_SliderTickmarks : QStyle::SC_None);
	style()->drawComplexControl(QStyle::CC_Slider, &opt, &painter, this);
	QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);

	// Draw span
	initStyleOption(&opt);
	opt.subControls    = QStyle::SC_SliderGroove;
	opt.sliderValue    = 0;
	opt.sliderPosition = m_low;
	QRect lowRect      = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
	opt.sliderPosition = m_high;
	QRect highRect     = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

	int lowPos  = pick(lowRect.center());
	int highPos = pick(highRect.center());
	int minPos  = std::min(lowPos, highPos);
	int maxPos  = std::max(lowPos, highPos);

	QPoint c = QRect(lowRect.center(), highRect.center()).center();
	QRect  spanRect;
	if (opt.orientation == Qt::Horizontal)
	{
		spanRect = QRect(QPoint(minPos, c.y() - 2), QPoint(maxPos, c.y() + 1));
		groove.adjust(0, 0, -1, 0);
	}
	else
	{
		spanRect = QRect(QPoint(c.x() - 2, minPos), QPoint(c.x() + 1, maxPos));
		groove.adjust(0, 0, 0, -1);
	}

	{
		QColor highlight = palette().color(QPalette::Highlight);
		painter.setBrush(QBrush(highlight));
		painter.setPen(QPen(highlight, 0));
		painter.drawRect(spanRect.intersected(groove));
	}

	const auto drawHandle = [&](const int limit) {
		initStyleOption(&opt);
		opt.subControls       = QStyle::SC_SliderHandle | (tickPosition() != NoTicks ? QStyle::SC_SliderTickmarks : QStyle::SC_None);
		opt.activeSubControls = m_pressedControl ? m_pressedControl : m_hoverControl;
		opt.sliderPosition    = limit;
		opt.sliderValue       = limit;
		style()->drawComplexControl(QStyle::CC_Slider, &opt, &painter, this);
	};

	drawHandle(m_low);
	drawHandle(m_high);
}

void RangeSlider::mousePressEvent(QMouseEvent* ev)
{
	const ScopedCall pressGuard([&] {
		QSlider::mousePressEvent(ev);
	});

	if (ev->button() != Qt::LeftButton)
		return ev->ignore();

	ev->accept();
	QStyleOptionSlider opt;
	initStyleOption(&opt);
	m_activeSlider   = -1;
	m_pressedControl = QStyle::SC_SliderHandle;

	const ScopedCall actionGuard([&] {
		triggerAction(SliderMove);
		setRepeatAction(SliderNoAction);
	});

	const auto setActiveSlider = [this](const int sliderId) {
		m_activeSlider = sliderId;
		setSliderDown(true);
	};

	const auto checkActiveSlider = [&](const int limit, const int sliderId) {
		opt.sliderPosition = limit;
		if (style()->hitTestComplexControl(QStyle::CC_Slider, &opt, ev->pos(), this) != QStyle::SC_SliderHandle)
			return false;

		return setActiveSlider(sliderId), true;
	};

	if (checkActiveSlider(m_low, 0) || checkActiveSlider(m_high, 1))
		return;

	const auto newPos = pixelPosToRangeValue(pick(ev->pos()));

	if (Util::InBounds(newPos, minimum(), m_low))
	{
		m_low = newPos;
		return setActiveSlider(0);
	}

	if (Util::InBounds(newPos, m_high, maximum()))
	{
		m_high = newPos;
		return setActiveSlider(1);
	}
}

void RangeSlider::mouseMoveEvent(QMouseEvent* ev)
{
	if (m_pressedControl != QStyle::SC_SliderHandle)
		return ev->ignore();

	ev->accept();
	int newPos = pixelPosToRangeValue(pick(ev->pos()));

	QStyleOptionSlider opt;
	initStyleOption(&opt);

	const ScopedCall updateGuard([&] {
		update();
		emit sliderMoved(m_low, m_high);
	});

	if (m_activeSlider == 0)
	{
		newPos = std::min(newPos, m_high - 1);
		m_low  = newPos;
		return;
	}

	if (m_activeSlider == 1)
	{
		newPos = std::max(newPos, m_low + 1);
		m_high = newPos;
		return;
	}
}

int RangeSlider::pick(const QPoint& pt) const
{
	return orientation() == Qt::Horizontal ? pt.x() : pt.y();
}

int RangeSlider::pixelPosToRangeValue(const int pos) const
{
	QStyleOptionSlider opt;
	initStyleOption(&opt);

	const auto gr = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
	const auto sr = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

	const auto [sliderLength, sliderMin, sliderMax] = orientation() == Qt::Horizontal ? std::make_tuple(sr.width(), gr.x(), gr.right()) : std::make_tuple(sr.height(), gr.y(), gr.bottom());
	return QStyle::sliderValueFromPosition(minimum(), maximum(), pos - sliderMin - sliderLength / 2, sliderMax - sliderLength + 1 - sliderMin, opt.upsideDown);
}
