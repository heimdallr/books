#include "CommandArgDelegate.h"

#include <ranges>

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
	for (const auto & item : IScriptController::s_commandMacros | std::views::values)
	{
		const auto menuItemTitle = QString("%1\t%2").arg(Loc::Tr(IScriptController::s_context, item), item);
		editor->addAction(menuItemTitle, [=, value = QString(item)]
		{
			auto currentText = lineEdit->text();
			const auto currentPosition = lineEdit->cursorPosition();
			lineEdit->setText(currentText.insert(currentPosition, value));
			lineEdit->setCursorPosition(currentPosition + static_cast<int>(value.size()));
		});
	}
	return editor;
}
