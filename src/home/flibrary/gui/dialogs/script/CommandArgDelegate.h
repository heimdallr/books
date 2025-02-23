#pragma once

#include <memory>

#include "LineEditDelegate.h"

namespace HomeCompa::Flibrary
{

class CommandArgDelegate final : public LineEditDelegate
{
public:
	explicit CommandArgDelegate(QObject* parent = nullptr);

private: // QStyledItemDelegate
	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

}
