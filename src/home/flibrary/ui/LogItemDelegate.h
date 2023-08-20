#pragma once

#include <QStyledItemDelegate>
#include "interface/constants/PLogSeverityLocalization.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class LogItemDelegate final : public QStyledItemDelegate
{
public:
	explicit LogItemDelegate(const std::shared_ptr<ISettings> & settings
		, QObject * parent = nullptr
	);

private: // QStyledItemDelegate
	void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override;

private:
	QColor m_colors[std::size(SEVERITIES)];
};

}
