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

class IDelegateGetter // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IDelegateGetter()                                            = default;
	virtual BaseDelegateEditor* GetOpenFileDialogDelegateEditor() const   = 0;
	virtual BaseDelegateEditor* GetStorableComboboxDelegateEditor() const = 0;
	virtual BaseDelegateEditor* GetEmbeddedCommandsDelegateEditor() const = 0;
};

constexpr std::pair<IScriptController::Command::Type, BaseDelegateEditor* (IDelegateGetter::*)() const> TYPES[] {
	{ IScriptController::Command::Type::LaunchConsoleApp,   &IDelegateGetter::GetOpenFileDialogDelegateEditor },
	{     IScriptController::Command::Type::LaunchGuiApp,   &IDelegateGetter::GetOpenFileDialogDelegateEditor },
	{		   IScriptController::Command::Type::System, &IDelegateGetter::GetStorableComboboxDelegateEditor },
	{		 IScriptController::Command::Type::Embedded, &IDelegateGetter::GetEmbeddedCommandsDelegateEditor },
};
static_assert(std::size(TYPES) == static_cast<size_t>(IScriptController::Command::Type::Last));

} // namespace

class CommandDelegate::Impl final : public IDelegateGetter
{
	using Editors = std::vector<std::pair<IScriptController::Command::Type, BaseDelegateEditor*>>;

public:
	Impl(
		std::shared_ptr<OpenFileDialogDelegateEditor>   openFileDialogDelegateEditor,
		std::shared_ptr<StorableComboboxDelegateEditor> storableComboboxDelegateEditor,
		std::shared_ptr<EmbeddedCommandsDelegateEditor> embeddedCommandsDelegateEditor
	)
		: m_openFileDialogDelegateEditor { std::move(openFileDialogDelegateEditor) }
		, m_storableComboboxDelegateEditor { CreateStorableComboboxDelegateEditor(std::move(storableComboboxDelegateEditor)) }
		, m_embeddedCommandsDelegateEditor { std::move(embeddedCommandsDelegateEditor) }
		, m_editors(CreateEditors())
	{
	}

	BaseDelegateEditor* GetEditor(const QModelIndex& index) const
	{
		return FindSecond(m_editors, static_cast<IScriptController::Command::Type>(index.data(Role::Type).toInt()));
	}

private: // IDelegateGetter
	BaseDelegateEditor* GetOpenFileDialogDelegateEditor() const override
	{
		return m_openFileDialogDelegateEditor.get();
	}

	BaseDelegateEditor* GetStorableComboboxDelegateEditor() const override
	{
		return m_storableComboboxDelegateEditor.get();
	}

	BaseDelegateEditor* GetEmbeddedCommandsDelegateEditor() const override
	{
		return m_embeddedCommandsDelegateEditor.get();
	}

private:
	Editors CreateEditors() const
	{
		Editors editors;
		editors.reserve(std::size(TYPES));
		std::ranges::transform(TYPES, std::back_inserter(editors), [this](const auto& item) {
			return std::make_pair(item.first, std::invoke(item.second, this));
		});
		return editors;
	}

private:
	std::shared_ptr<BaseDelegateEditor> m_openFileDialogDelegateEditor;
	std::shared_ptr<BaseDelegateEditor> m_storableComboboxDelegateEditor;
	std::shared_ptr<BaseDelegateEditor> m_embeddedCommandsDelegateEditor;
	const Editors                       m_editors;
};

CommandDelegate::CommandDelegate(
	std::shared_ptr<OpenFileDialogDelegateEditor>   openFileDialogDelegateEditor,
	std::shared_ptr<StorableComboboxDelegateEditor> storableComboboxDelegateEditor,
	std::shared_ptr<EmbeddedCommandsDelegateEditor> embeddedCommandsDelegateEditor,
	QObject*                                        parent
)
	: QStyledItemDelegate(parent)
	, m_impl(std::make_unique<Impl>(std::move(openFileDialogDelegateEditor), std::move(storableComboboxDelegateEditor), std::move(embeddedCommandsDelegateEditor)))
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
	editor->SetText(index.data(Role::Name).toString());
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
