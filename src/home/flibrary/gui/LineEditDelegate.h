#pragma once

#include <QStyledItemDelegate>

namespace HomeCompa::Flibrary {

class LineEditDelegate
	: public QStyledItemDelegate
{
public:
	explicit LineEditDelegate(QObject * parent = nullptr);

protected: // QStyledItemDelegate
	QWidget * createEditor(QWidget * parent, const QStyleOptionViewItem & option, const QModelIndex & index) const override;
	void setEditorData(QWidget * editor, const QModelIndex & index) const override;
	void setModelData(QWidget * editor, QAbstractItemModel * model, const QModelIndex & index) const override;
};

}
