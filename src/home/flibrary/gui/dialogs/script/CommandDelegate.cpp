#include "CommandDelegate.h"

#include <QMouseEvent>

#include "fnd/FindPair.h"

#include "interface/logic/IScriptController.h"

using namespace HomeCompa::Flibrary;

namespace
{

using Role = IScriptController::RoleCommand;

constexpr auto SYS_COMMAND_KEY_TEMPLATE = "ui/ScriptDialog/Commands";

std::shared_ptr<BaseDelegateEditor> CreateStorableComboboxDelegateEditor(std::shared_ptr<StorableComboboxDelegateEditor> storableComboboxDelegateEditor)
{
	storableComboboxDelegateEditor->SetSettingsKey(SYS_COMMAND_KEY_TEMPLATE);
	return storableComboboxDelegateEditor;
}

}

class CommandDelegate::Impl
{
	using Editors = std::vector<std::pair<IScriptController::Command::Type, BaseDelegateEditor*>>;

public:
	Impl(std::shared_ptr<OpenFileDialogDelegateEditor> openFileDialogDelegateEditor, std::shared_ptr<StorableComboboxDelegateEditor> storableComboboxDelegateEditor)
		: m_openFileDialogDelegateEditor(std::move(openFileDialogDelegateEditor))
		, m_storableComboboxDelegateEditor(CreateStorableComboboxDelegateEditor(std::move(storableComboboxDelegateEditor)))
		, m_editors(CreateEditors())
	{
	}

	BaseDelegateEditor* GetEditor(const QModelIndex& index) const
	{
		return FindSecond(m_editors, static_cast<IScriptController::Command::Type>(index.data(Role::Type).toInt()));
	}

private:
	Editors CreateEditors() const
	{
		return Editors {
			{ IScriptController::Command::Type::LaunchApp,   m_openFileDialogDelegateEditor.get() },
			{    IScriptController::Command::Type::System, m_storableComboboxDelegateEditor.get() },
		};
	}

private:
	std::shared_ptr<BaseDelegateEditor> m_openFileDialogDelegateEditor;
	std::shared_ptr<BaseDelegateEditor> m_storableComboboxDelegateEditor;
	const Editors m_editors;
};

CommandDelegate::CommandDelegate(std::shared_ptr<OpenFileDialogDelegateEditor> openFileDialogDelegateEditor, std::shared_ptr<StorableComboboxDelegateEditor> storableComboboxDelegateEditor, QObject* parent)
	: QStyledItemDelegate(parent)
	, m_impl(std::make_unique<Impl>(std::move(openFileDialogDelegateEditor), std::move(storableComboboxDelegateEditor)))
{
}

CommandDelegate::~CommandDelegate() = default;

QWidget* CommandDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex& index) const
{
	auto* editor = m_impl->GetEditor(index);
	editor->SetParent(parent);
	return editor->GetWidget();
}

void CommandDelegate::setEditorData(QWidget* /*editor*/, const QModelIndex& index) const
{
	auto* editor = m_impl->GetEditor(index);
	editor->SetText(index.data().toString());
}

void CommandDelegate::setModelData(QWidget* /*editor*/, QAbstractItemModel* model, const QModelIndex& index) const
{
	auto* editor = m_impl->GetEditor(index);
	editor->SetModel(model, index);
	model->setData(index, editor->GetText());
	editor->OnSetModelData(editor->GetText());
}

void CommandDelegate::destroyEditor(QWidget*, const QModelIndex& index) const
{
	auto* editor = m_impl->GetEditor(index);
	editor->SetParent(nullptr);
}
