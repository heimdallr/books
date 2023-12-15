#include "BaseDelegateEditor.h"

#include <QModelIndex>

using namespace HomeCompa::Flibrary;

BaseDelegateEditor::BaseDelegateEditor(QWidget * self, QWidget * parent)
	: QWidget(parent)
	, m_self(self)
{
}

QWidget * BaseDelegateEditor::GetWidget() const noexcept
{
	return m_self;
}

void BaseDelegateEditor::SetParent(QWidget * parent)
{
	m_self->setParent(parent);
}

void BaseDelegateEditor::SetModel(QAbstractItemModel * model, const QModelIndex & index)
{
	m_model = model;
	m_row = index.row();
	m_column = index.column();
}
