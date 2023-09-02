#include "LogItemDelegate.h"

#include <plog/Log.h>

#include "interface/logic/LogModelRole.h"
#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

namespace {
constexpr auto COLOR_KEY = "ui/Colors/Log/%1";
}

LogItemDelegate::LogItemDelegate(const std::shared_ptr<ISettings> & settings
	, QObject * parent
)
	: QStyledItemDelegate(parent)
{
	std::ranges::transform(SEVERITIES, std::begin(m_colors), [&] (const auto & severity)
	{
		const auto key = QString(COLOR_KEY).arg(severity.first);
		if (const auto colorInt = settings->Get(key, 0); colorInt != 0)
			return QColor(colorInt & 0x000000FF, (colorInt & 0x0000FF00) >> 8, (colorInt & 0x00FF0000) >> 16, (colorInt & 0xFF000000) >> 24);

		settings->Set(key, severity.second.red() | (severity.second.green() << 8) | (severity.second.blue() << 16) | (severity.second.alpha() << 24));
		return severity.second;
	});
}

void LogItemDelegate::paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
	auto o = option;
	o.palette.setColor(QPalette::Text, m_colors[index.data(LogModelRole::Severity).toInt()]);
	QStyledItemDelegate::paint(painter, o, index);
}
