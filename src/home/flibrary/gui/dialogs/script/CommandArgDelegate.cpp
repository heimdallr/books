#include "CommandArgDelegate.h"

#include <QLinEEdit>

#include "interface/constants/Localization.h"
#include "interface/logic/IScriptController.h"

using namespace HomeCompa::Flibrary;

CommandArgDelegate::CommandArgDelegate(QObject * parent)
	: LineEditDelegate(parent)
{
}

QWidget * CommandArgDelegate::createEditor(QWidget * parent, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
	auto* editor = LineEditDelegate::createEditor(parent, option, index);
	editor->setContextMenuPolicy(Qt::ActionsContextMenu);
	auto * lineEdit = qobject_cast<QLineEdit *>(editor);
	IScriptController::SetMacroActions(lineEdit);
	return editor;
}
