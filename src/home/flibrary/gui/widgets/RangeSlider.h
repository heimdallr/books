#pragma once

#include <QMouseEvent>
#include <QSlider>
#include <QStyle>

namespace HomeCompa::Flibrary
{

class RangeSlider final : public QSlider
{
	Q_OBJECT

signals:
	void sliderMoved(int, int) const;

public:
	explicit RangeSlider(QWidget* parent = nullptr);

	int  low() const noexcept;
	void setLow(int lowLimit) noexcept;
	int  high() const noexcept;
	void setHigh(int highLimit) noexcept;

private: // QWidget
	void paintEvent(QPaintEvent* ev) override;
	void mousePressEvent(QMouseEvent* ev) override;
	void mouseMoveEvent(QMouseEvent* ev) override;

private:
	int pick(const QPoint& pt) const;
	int pixelPosToRangeValue(int pos) const;

private:
	int m_low, m_high;

	QStyle::SubControl    m_pressedControl = QStyle::SC_None;
	int                   m_tickInterval   = 0;
	QSlider::TickPosition m_tickPosition   = QSlider::NoTicks;
	QStyle::SubControl    m_hoverControl   = QStyle::SC_None;
	int                   m_activeSlider   = 0;
};

} // namespace HomeCompa::Flibrary
