#include "CommandDelegate.h"

#include <QApplication>
#include <QMouseEvent>

#include "fnd/FindPair.h"
#include "interface/logic/IScriptController.h"

#include "delegate/OpenFileDialogDelegateEditor.h"

using namespace HomeCompa::Flibrary;

namespace {

using Role = IScriptController::RoleCommand;

}

class CommandDelegate::Impl
{
	using Editors = std::vector<std::pair<IScriptController::Command::Type, BaseDelegateEditor*>>;

public:
	Impl(std::shared_ptr<OpenFileDialogDelegateEditor> openFileDialogDelegateEditor)
		: m_openFileDialogDelegateEditor(std::move(openFileDialogDelegateEditor))
		, m_editors(CreateEditors())
	{
	}

	BaseDelegateEditor * GetEditor(const QModelIndex & index) const
	{
		return FindSecond(m_editors, static_cast<IScriptController::Command::Type>(index.data(Role::Type).toInt()));
	}

private:
	Editors CreateEditors() const
	{
		return Editors {
			{ IScriptController::Command::Type::LaunchApp, m_openFileDialogDelegateEditor.get() },
			{ IScriptController::Command::Type::System   , m_openFileDialogDelegateEditor.get() },
		};
	}

private:
	std::shared_ptr<OpenFileDialogDelegateEditor> m_openFileDialogDelegateEditor;
	const Editors m_editors;
};

CommandDelegate::CommandDelegate(std::shared_ptr<OpenFileDialogDelegateEditor> openFileDialogDelegateEditor
	, QObject * parent
)
	: QStyledItemDelegate(parent)
	, m_impl(std::make_unique<Impl>(std::move(openFileDialogDelegateEditor)))
{
}

CommandDelegate::~CommandDelegate() = default;

QWidget * CommandDelegate::createEditor(QWidget * parent, const QStyleOptionViewItem &, const QModelIndex & index) const
{
	auto * editor = m_impl->GetEditor(index);
	editor->SetParent(parent);
	return editor->GetWidget();
}

void CommandDelegate::setEditorData(QWidget * /*editor*/, const QModelIndex & index) const
{
	auto * editor = m_impl->GetEditor(index);
	editor->SetText(index.data().toString());
}

void CommandDelegate::setModelData(QWidget * /*editor*/, QAbstractItemModel * model, const QModelIndex & index) const
{
	auto * editor = m_impl->GetEditor(index);
	editor->SetModel(model, index);
	model->setData(index, editor->GetText());
}

void CommandDelegate::destroyEditor(QWidget *, const QModelIndex & index) const
{
	auto * editor = m_impl->GetEditor(index);
	editor->SetParent(nullptr);
}
