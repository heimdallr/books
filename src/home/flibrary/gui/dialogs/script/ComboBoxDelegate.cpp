#include "ComboBoxDelegate.h"

#include <QComboBox>

#include "interface/logic/IScriptController.h"

using namespace HomeCompa::Flibrary;

namespace
{
using Role = IScriptController::RoleBase;
}

ComboBoxDelegate::ComboBoxDelegate(QObject* parent)
	: QStyledItemDelegate(parent)
{
}

QWidget* ComboBoxDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const
{
	auto* editor = new QComboBox(parent);
	for (const auto& [id, name] : m_values)
		editor->addItem(name, id);
	return editor;
}

void ComboBoxDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	auto*      comboBox     = qobject_cast<QComboBox*>(editor);
	const auto currentIndex = comboBox->findData(index.data(Role::Type));
	comboBox->setCurrentIndex(currentIndex);
}

void ComboBoxDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	const auto* comboBox = qobject_cast<QComboBox*>(editor);
	model->setData(index, comboBox->currentData());
}

void ComboBoxDelegate::SetValues(Values values)
{
	m_values = std::move(values);
}
