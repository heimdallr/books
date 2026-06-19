#include "RangeSlider.h"

#include <QApplication>
#include <QPainter>
#include <QStyleOptionSlider>

using namespace HomeCompa::Flibrary;

RangeSlider::RangeSlider(QWidget* parent)
	: QSlider(parent)
	, m_lowLimit { minimum() }
	, m_highLimit { maximum() }
{
}

int RangeSlider::low() const noexcept
{
	return m_lowLimit;
}

void RangeSlider::setLow(const int lowLimit) noexcept
{
	m_lowLimit = lowLimit;
	update();
}

int RangeSlider::high() const noexcept
{
	return m_highLimit;
}

void RangeSlider::setHigh(const int highLimit) noexcept
{
	m_highLimit = highLimit;
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
	opt.subControls    = QStyle::SC_SliderGroove;
	if (tickPosition() != NoTicks)
		opt.subControls |= QStyle::SC_SliderTickmarks;
	style()->drawComplexControl(QStyle::CC_Slider, &opt, &painter, this);
	QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);

	// Draw span
	initStyleOption(&opt);
	opt.subControls    = QStyle::SC_SliderGroove;
	opt.sliderValue    = 0;
	opt.sliderPosition = m_lowLimit;
	QRect low_rect     = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
	opt.sliderPosition = m_highLimit;
	QRect high_rect    = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

	int low_pos  = pick(low_rect.center());
	int high_pos = pick(high_rect.center());
	int min_pos  = std::min(low_pos, high_pos);
	int max_pos  = std::max(low_pos, high_pos);

	QPoint c = QRect(low_rect.center(), high_rect.center()).center();
	QRect  span_rect;
	if (opt.orientation == Qt::Horizontal)
	{
		span_rect = QRect(QPoint(min_pos, c.y() - 2), QPoint(max_pos, c.y() + 1));
		groove.adjust(0, 0, -1, 0);
	}
	else
	{
		span_rect = QRect(QPoint(c.x() - 2, min_pos), QPoint(c.x() + 1, max_pos));
		groove.adjust(0, 0, 0, -1);
	}

	if (1)
	{
		QColor highlight = palette().color(QPalette::Highlight);
		painter.setBrush(QBrush(highlight));
		painter.setPen(QPen(highlight, 0));
		painter.drawRect(span_rect.intersected(groove));
		//.color(QPalette.)
	}

	// FIXED: use self.style(), not QApplication.style() by MaurizioB
	//        QStyle* style = QApplication::style();
	initStyleOption(&opt);
	opt.subControls = QStyle::SC_SliderHandle; // | QStyle::SC_SliderGroove;
	if (tickPosition() != QSlider::NoTicks)
		opt.subControls |= QStyle::SC_SliderTickmarks;
	if (m_pressedControl)
		opt.activeSubControls = m_pressedControl;
	else
		opt.activeSubControls = m_hoverControl;
	opt.sliderPosition = m_lowLimit;
	opt.sliderValue    = m_lowLimit;
	style()->drawComplexControl(QStyle::CC_Slider, &opt, &painter, this);

	initStyleOption(&opt);
	opt.subControls = QStyle::SC_SliderHandle;
	if (tickPosition() != QSlider::NoTicks)
		opt.subControls |= QStyle::SC_SliderTickmarks;
	if (m_pressedControl)
		opt.activeSubControls = m_pressedControl;
	else
		opt.activeSubControls = m_hoverControl;
	opt.sliderPosition = m_highLimit;
	opt.sliderValue    = m_highLimit;
	style()->drawComplexControl(QStyle::CC_Slider, &opt, &painter, this);
}

void RangeSlider::mousePressEvent(QMouseEvent* ev)
{
	/*  # In a normal slider control, when the user clicks on a point in the
            # slider's total range, but not on the slider part of the control the
            # control would jump the slider value to where the user clicked.
            # For this control, clicks which are not direct hits will slide both
            # slider parts
            */
	if (ev->button() == Qt::LeftButton)
	{
		ev->accept();
		QStyleOptionSlider opt;
		initStyleOption(&opt);
		m_activeSlider = -1;

		QStyle*            style = QApplication::style();
		QStyle::SubControl hit;

		opt.sliderPosition = m_lowLimit;
		hit                = style->hitTestComplexControl(QStyle::CC_Slider, &opt, ev->pos(), this);
		if (hit == QStyle::SC_SliderHandle)
		{
			m_activeSlider   = 0;
			m_pressedControl = hit;
			triggerAction(SliderMove);
			setRepeatAction(SliderNoAction);
			setSliderDown(true);
		}
		else
		{
			opt.sliderPosition = m_highLimit;
			hit                = style->hitTestComplexControl(QStyle::CC_Slider, &opt, ev->pos(), this);
			if (hit == QStyle::SC_SliderHandle)
			{
				m_activeSlider   = 1;
				m_pressedControl = hit;
				triggerAction(SliderMove);
				setRepeatAction(SliderNoAction);
				setSliderDown(true);
			}
		}

		if (m_activeSlider < 0)
		{
			m_pressedControl = QStyle::SC_SliderHandle;
			m_clickOffset    = pixelPosToRangeValue(pick(ev->pos()));
			triggerAction(SliderMove);
			setRepeatAction(SliderNoAction);
		}
	}
	else
	{
		ev->ignore();
	}

	QSlider::mousePressEvent(ev);
}

void RangeSlider::mouseMoveEvent(QMouseEvent* ev)
{
	if (m_pressedControl != QStyle::SC_SliderHandle)
	{
		ev->ignore();
		return;
	}

	ev->accept();
	int                new_pos = pixelPosToRangeValue(pick(ev->pos()));
	QStyleOptionSlider opt;
	initStyleOption(&opt);

	int offset, diff;
	if (m_activeSlider < 0)
	{
		offset       = new_pos - m_clickOffset;
		m_highLimit += offset;
		m_lowLimit  += offset;
		if (m_lowLimit < minimum())
		{
			diff         = minimum() - m_lowLimit;
			m_lowLimit  += diff;
			m_highLimit += diff;
		}
		if (m_highLimit > maximum())
		{
			diff         = maximum() - m_highLimit;
			m_lowLimit  += diff;
			m_highLimit += diff;
		}
	}
	else if (m_activeSlider == 0)
	{
		if (new_pos >= m_highLimit)
			new_pos = m_highLimit - 1;
		m_lowLimit = new_pos;
	}
	else
	{
		if (new_pos <= m_lowLimit)
			new_pos = m_lowLimit + 1;
		m_highLimit = new_pos;
	}

	m_clickOffset = new_pos;
	update();
	emit sliderMoved(m_lowLimit, m_highLimit);
}

int RangeSlider::pick(const QPoint& pt) const
{
	return orientation() == Qt::Horizontal ? pt.x() : pt.y();
}

int RangeSlider::pixelPosToRangeValue(int pos) const
{
	QStyleOptionSlider opt;
	initStyleOption(&opt);
	QStyle* style = QApplication::style();

	QRect gr = style->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
	QRect sr = style->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

	int slider_length, slider_min, slider_max;
	if (orientation() == Qt::Horizontal)
	{
		slider_length = sr.width();
		slider_min    = gr.x();
		slider_max    = gr.right() - slider_length + 1;
	}
	else
	{
		slider_length = sr.height();
		slider_min    = gr.y();
		slider_max    = gr.bottom() - slider_length + 1;
	}

	return QStyle::sliderValueFromPosition(minimum(), maximum(), pos - slider_min, slider_max - slider_min, opt.upsideDown);
}
