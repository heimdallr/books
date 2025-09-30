#include "CommandArgDelegate.h"

#include <QLineEdit>

#include "interface/logic/IScriptController.h"

using namespace HomeCompa::Flibrary;

CommandArgDelegate::CommandArgDelegate(QObject* parent)
	: LineEditDelegate(parent)
{
}

QWidget* CommandArgDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	auto* editor = LineEditDelegate::createEditor(parent, option, index);
	editor->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(editor, &QWidget::customContextMenuRequested, this, [editor] {
		IScriptController::ExecuteContextMenu(qobject_cast<QLineEdit*>(editor));
	});
	return editor;
}
