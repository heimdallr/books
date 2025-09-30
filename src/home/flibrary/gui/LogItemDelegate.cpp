#include "LogItemDelegate.h"

#include <ranges>

#include <QGuiApplication>

#include "interface/logic/LogModelRole.h"

using namespace HomeCompa::Flibrary;

LogItemDelegate::LogItemDelegate(QObject* parent)
	: QStyledItemDelegate(parent)
{
	const auto  palette    = QGuiApplication::palette();
	const auto& severities = palette.color(QPalette::WindowText).lightness() > palette.color(QPalette::Window).lightness() ? SEVERITIES_DARK : SEVERITIES;
	std::ranges::copy(severities | std::views::values, std::begin(m_colors));
}

void LogItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	auto o = option;
	o.palette.setColor(QPalette::Text, m_colors[index.data(LogModelRole::Severity).toInt()]);
	QStyledItemDelegate::paint(painter, o, index);
}
