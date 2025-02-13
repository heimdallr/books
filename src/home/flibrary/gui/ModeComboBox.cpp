#include "ModeComboBox.h"

#include <ranges>

#include <QGuiApplication>
#include <QPainter>

#include "fnd/ScopedCall.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr auto VALUE_MODE_ICON_TEMPLATE = ":/icons/%1.svg";

}

ModeComboBox::ModeComboBox(QWidget* parent)
	: QComboBox(parent)
{
	for (const auto* name : VALUE_MODES | std::views::keys)
		addItem(QIcon(QString(VALUE_MODE_ICON_TEMPLATE).arg(name)), {});
}

void ModeComboBox::paintEvent(QPaintEvent* event)
{
	QComboBox::paintEvent(event);
	QPainter p;
	const ScopedCall painterGuard([&] { p.begin(this); }, [&] { p.end(); });

	QStyleOptionComboBox opt;
	opt.initFrom(this);
	style()->drawPrimitive(QStyle::PE_PanelButtonBevel, &opt, &p, this);
	style()->drawPrimitive(QStyle::PE_PanelButtonCommand, &opt, &p, this);

	QIcon icon(QString(VALUE_MODE_ICON_TEMPLATE).arg(VALUE_MODES[currentIndex()].first));
	const auto adjust = opt.rect.size() / 6;
	const auto adjustedRect = opt.rect.adjusted(adjust.width(), adjust.height(), -adjust.width(), -adjust.height());
	p.drawPixmap(adjustedRect.topLeft(), icon.pixmap(adjustedRect.size()));
}
