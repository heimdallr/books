#pragma once

#include <QStyledItemDelegate>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace HomeCompa::Flibrary
{

class CommandDelegate : public QStyledItemDelegate
{
	NON_COPY_MOVABLE(CommandDelegate)

public:
	CommandDelegate(std::shared_ptr<class OpenFileDialogDelegateEditor> openFileDialogDelegateEditor,
	                std::shared_ptr<class StorableComboboxDelegateEditor> storableComboboxDelegateEditor,
	                QObject* parent = nullptr);
	~CommandDelegate() override;

private: // QStyledItemDelegate
	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void setEditorData(QWidget* editor, const QModelIndex& index) const override;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
	void destroyEditor(QWidget* editor, const QModelIndex& index) const override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
