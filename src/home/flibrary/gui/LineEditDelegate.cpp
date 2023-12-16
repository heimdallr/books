#include "LineEditDelegate.h"

#include <QLineEdit>

using namespace HomeCompa::Flibrary;

LineEditDelegate::LineEditDelegate(QObject * parent)
	: QStyledItemDelegate(parent)
{
}

QWidget * LineEditDelegate::createEditor(QWidget * parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
	return new QLineEdit(parent);
}

void LineEditDelegate::setEditorData(QWidget * editor, const QModelIndex & index) const
{
	auto * lineEdit = qobject_cast<QLineEdit *>(editor);
	lineEdit->setText(index.data().toString());
}

void LineEditDelegate::setModelData(QWidget * editor, QAbstractItemModel * model, const QModelIndex & index) const
{
	const auto * lineEdit = qobject_cast<QLineEdit *>(editor);
	model->setData(index, lineEdit->text());
}
