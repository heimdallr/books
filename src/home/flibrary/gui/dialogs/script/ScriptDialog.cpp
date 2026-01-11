#include "ui_ScriptDialog.h"

#include "ScriptDialog.h"

#include <ranges>
#include <set>

#include "fnd/FindPair.h"
#include "fnd/algorithm.h"

#include "interface/Localization.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IScriptController.h"

#include "gutil/GeometryRestorable.h"
#include "util/files.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

using Role = IScriptController::RoleBase;

constexpr auto COLUMN_WIDTH_KEY_TEMPLATE = "ui/%1/%2/Column%3Width";
constexpr auto SYS_COMMAND_KEY           = "ui/ScriptDialog/Commands";
constexpr auto DIALOG_KEY                = "Script";

constexpr auto CONTEXT             = "ScriptEditor";
constexpr auto APP_FILE_FILTER     = QT_TRANSLATE_NOOP("ScriptEditor", "Applications (*.exe);;Scripts (*.bat *.cmd);;All files (*.*)");
constexpr auto FILE_DIALOG_TITLE   = QT_TRANSLATE_NOOP("ScriptEditor", "Select Application");
constexpr auto FOLDER_DIALOG_TITLE = QT_TRANSLATE_NOOP("ScriptEditor", "Select working folder");

TR_DEF

QString FromLineEdit(const std::vector<QWidget*>& widgets, const IScriptController::Command::Type type)
{
	const auto index = static_cast<size_t>(type);
	assert(index < widgets.size());
	return qobject_cast<QLineEdit*>(widgets[index])->text();
}

void ToLineEdit(const std::vector<QWidget*>& widgets, const IScriptController::Command::Type type, const QVariant& value)
{
	const auto index = static_cast<size_t>(type);
	assert(index < widgets.size());
	qobject_cast<QLineEdit*>(widgets[index])->setText(value.toString());
}

QString FromComboBoxText(const std::vector<QWidget*>& widgets, const IScriptController::Command::Type type)
{
	const auto index = static_cast<size_t>(type);
	assert(index < widgets.size());
	return qobject_cast<QComboBox*>(widgets[index])->currentText();
}

void ToComboBoxText(const std::vector<QWidget*>& widgets, const IScriptController::Command::Type type, const QVariant& value)
{
	const auto index = static_cast<size_t>(type);
	assert(index < widgets.size());
	qobject_cast<QComboBox*>(widgets[index])->setCurrentText(value.toString());
}

QString FromComboBoxData(const std::vector<QWidget*>& widgets, const IScriptController::Command::Type type)
{
	const auto index = static_cast<size_t>(type);
	assert(index < widgets.size());
	return qobject_cast<QComboBox*>(widgets[index])->currentText();
}

void ToComboBoxData(const std::vector<QWidget*>& widgets, const IScriptController::Command::Type type, const QVariant& value)
{
	const auto index = static_cast<size_t>(type);
	assert(index < widgets.size());
	auto* comboBox = qobject_cast<QComboBox*>(widgets[index]);
	if (const auto comboBoxIndex = comboBox->findData(value); comboBoxIndex >= 0)
		comboBox->setCurrentIndex(comboBoxIndex);
}

using CommandGetter = QString (*)(const std::vector<QWidget*>& widgets, IScriptController::Command::Type type);
using CommandSetter = void (*)(const std::vector<QWidget*>& widgets, IScriptController::Command::Type type, const QVariant& value);

constexpr std::pair<IScriptController::Command::Type, std::pair<CommandGetter, CommandSetter>> COMMAND_GETTERS[] {
	{ IScriptController::Command::Type::LaunchConsoleApp,         { &FromLineEdit, &ToLineEdit } },
	{     IScriptController::Command::Type::LaunchGuiApp,         { &FromLineEdit, &ToLineEdit } },
	{		   IScriptController::Command::Type::System, { &FromComboBoxText, &ToComboBoxText } },
	{		 IScriptController::Command::Type::Embedded, { &FromComboBoxData, &ToComboBoxData } },
};
static_assert(std::size(COMMAND_GETTERS) == static_cast<size_t>(IScriptController::Command::Type::Last));

void RemoveRows(const QAbstractItemView& view)
{
	std::set<int> rows;
	std::ranges::transform(view.selectionModel()->selection().indexes(), std::inserter(rows, rows.end()), [](const QModelIndex& index) {
		return index.row();
	});
	for (const auto& [begin, end] : Util::CreateRanges(rows) | std::views::reverse)
		view.model()->removeRows(begin, end - begin);
}

void SaveLayout(const QObject& parent, const QTableView& view, ISettings& settings)
{
	for (int i = 0, sz = view.horizontalHeader()->count() - 1; i < sz; ++i)
		settings.Set(QString(COLUMN_WIDTH_KEY_TEMPLATE).arg(parent.objectName()).arg(view.objectName()).arg(i), view.horizontalHeader()->sectionSize(i));
}

void LoadLayout(const QObject& parent, const QTableView& view, const ISettings& settings)
{
	for (int i = 0, sz = view.horizontalHeader()->count() - 1; i < sz; ++i)
		if (const auto width = settings.Get(QString(COLUMN_WIDTH_KEY_TEMPLATE).arg(parent.objectName()).arg(view.objectName()).arg(i)); width.isValid())
			view.horizontalHeader()->resizeSection(i, width.toInt());
}

template <std::ranges::forward_range R, class Proj = std::identity>
void SetupView(const QObject& parent, ISettings& settings, QTableView& view, QAbstractItemModel& model, ComboBoxDelegate& delegate, R&& array, Proj proj = {})
{
	view.setModel(&model);
	view.setItemDelegateForColumn(0, &delegate);
	ComboBoxDelegate::Values types;
	types.reserve(std::size(array));
	std::ranges::transform(array, std::back_inserter(types), [&](const auto& item) {
		return std::make_pair(static_cast<int>(item.first), Loc::Tr(IScriptController::s_context, std::invoke(proj, item.second)));
	});
	delegate.SetValues(std::move(types));

	LoadLayout(parent, view, settings);
}

} // namespace

class ScriptDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		ScriptDialog&                           self,
		const IModelProvider&                   modelProvider,
		std::shared_ptr<const Util::IUiFactory> uiFactory,
		std::shared_ptr<ISettings>              settings,
		std::shared_ptr<ComboBoxDelegate>       scriptTypeDelegate,
		std::shared_ptr<QStyledItemDelegate>    scriptNameLineEditDelegate
	)
		: GeometryRestorable(*this, settings, "ScriptDialog")
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_uiFactory { std::move(uiFactory) }
		, m_settings { std::move(settings) }
		, m_scriptModel { modelProvider.CreateScriptModel() }
		, m_commandModel { modelProvider.CreateScriptCommandModel() }
		, m_scriptTypeDelegate { std::move(scriptTypeDelegate) }
		, m_scriptNameLineEditDelegate { std::move(scriptNameLineEditDelegate) }
	{
		m_ui.setupUi(&m_self);

		m_commandTextWidgets.resize(std::size(IScriptController::s_commandTypes));
		m_commandTextWidgets[static_cast<size_t>(IScriptController::Command::Type::LaunchConsoleApp)] = m_ui.lineEditCommandTextExe;
		m_commandTextWidgets[static_cast<size_t>(IScriptController::Command::Type::LaunchGuiApp)]     = m_ui.lineEditCommandTextExe;
		m_commandTextWidgets[static_cast<size_t>(IScriptController::Command::Type::System)]           = m_ui.comboBoxCommandTextSystem;
		m_commandTextWidgets[static_cast<size_t>(IScriptController::Command::Type::Embedded)]         = m_ui.comboBoxCommandTextEmbedded;

		m_ui.lineEditCommandTextExe->addAction(m_ui.actionOpenExe, QLineEdit::TrailingPosition);
		m_ui.lineEditCommandWorkingFolder->addAction(m_ui.actionOpenCWD, QLineEdit::TrailingPosition);

		SetupView(m_self, *m_settings, *m_ui.viewScript, *m_scriptModel, *m_scriptTypeDelegate, IScriptController::s_scriptTypes);

		for (const auto& [commandType, commandDescription] : IScriptController::s_commandTypes)
			m_ui.comboBoxCommandType->addItem(Loc::Tr(IScriptController::s_context, commandDescription.type), QVariant::fromValue(commandType));
		OnComboBoxCommandTypeIndexChanged(m_ui.comboBoxCommandType->currentIndex());

		for (const auto* name : IScriptController::s_embeddedCommands | std::views::values)
			m_ui.comboBoxCommandTextEmbedded->addItem(Loc::Tr(IScriptController::s_context, name), QVariant::fromValue(QString { name }));

		if (auto commands = m_settings->Get(SYS_COMMAND_KEY).toStringList(); !commands.isEmpty())
		{
			std::ranges::sort(commands);
			for (const auto& command : commands)
				m_ui.comboBoxCommandTextSystem->addItem(command);
		}

		m_ui.viewScript->setItemDelegateForColumn(1, m_scriptNameLineEditDelegate.get());

		m_ui.viewCommand->setModel(m_commandModel.get());

		SetConnections();

		m_ui.viewScript->setCurrentIndex(m_scriptModel->index(0, 1));

		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
		SaveLayout(m_self, *m_ui.viewScript, *m_settings);
		SaveLayout(m_self, *m_ui.viewCommand, *m_settings);
	}

private:
	void SetConnections()
	{
		SetConnectionsScriptHead();
		SetConnectionsCommand();

		connect(m_ui.btnCancel, &QAbstractButton::clicked, &m_self, &QDialog::reject);
		connect(m_ui.btnSave, &QAbstractButton::clicked, &m_self, &QDialog::accept);
		connect(&m_self, &QDialog::accepted, &m_self, [this] {
			OnAccept();
		});
	}

	void SetConnectionsScriptHead()
	{
		connect(m_ui.viewScript->selectionModel(), &QItemSelectionModel::selectionChanged, &m_self, [this] {
			OnScriptSelectionChanged();
		});
		connect(m_scriptModel.get(), &QAbstractItemModel::modelReset, &m_self, [this] {
			OnScriptSelectionChanged();
		});
		connect(m_ui.btnAddScript, &QAbstractButton::clicked, &m_self, [this] {
			m_scriptModel->insertRow(m_scriptModel->rowCount());
			m_ui.viewScript->setCurrentIndex(m_scriptModel->index(m_scriptModel->rowCount() - 1, 1));
		});
		connect(m_ui.btnRemoveScript, &QAbstractButton::clicked, &m_self, [this] {
			RemoveRows(*m_ui.viewScript);
		});
		connect(m_ui.btnScriptUp, &QAbstractButton::clicked, &m_self, [this] {
			m_scriptModel->setData(m_ui.viewScript->currentIndex(), {}, Role::Up);
			OnScriptSelectionChanged();
		});
		connect(m_ui.btnScriptDown, &QAbstractButton::clicked, &m_self, [this] {
			m_scriptModel->setData(m_ui.viewScript->currentIndex(), {}, Role::Down);
			OnScriptSelectionChanged();
		});
	}

	void SetConnectionsCommand()
	{
		SetConnectionsCommandList();
		SetConnectionsCommandEdit();

		connect(m_ui.stackedWidgetCommand, &QStackedWidget::currentChanged, &m_self, [this](const int index) {
			m_ui.btnSave->setEnabled(index == 0);
		});
	}

	void SetConnectionsCommandList()
	{
		connect(m_ui.viewCommand->selectionModel(), &QItemSelectionModel::selectionChanged, &m_self, [this] {
			OnCommandSelectionChanged();
		});
		connect(m_commandModel.get(), &QAbstractItemModel::modelReset, &m_self, [this] {
			OnCommandSelectionChanged();
		});
		connect(m_ui.btnAddCommand, &QAbstractButton::clicked, &m_self, [this] {
			m_ui.stackedWidgetCommand->setCurrentWidget(m_ui.scriptCommandEditorPage);
			m_addMode = true;
		});
		connect(m_ui.btnEditCommand, &QAbstractButton::clicked, &m_self, [this] {
			OnEditCommandClicked();
			m_addMode = false;
		});
		connect(m_ui.btnRemoveCommand, &QAbstractButton::clicked, &m_self, [this] {
			RemoveRows(*m_ui.viewCommand);
		});
		connect(m_ui.btnCommandUp, &QAbstractButton::clicked, &m_self, [this] {
			m_commandModel->setData(m_ui.viewCommand->currentIndex(), {}, Role::Up);
			OnCommandSelectionChanged();
		});
		connect(m_ui.btnCommandDown, &QAbstractButton::clicked, &m_self, [this] {
			m_commandModel->setData(m_ui.viewCommand->currentIndex(), {}, Role::Down);
			OnCommandSelectionChanged();
		});
	}

	void SetConnectionsCommandEdit()
	{
		connect(m_ui.comboBoxCommandType, &QComboBox::currentIndexChanged, [this](const int index) {
			OnComboBoxCommandTypeIndexChanged(index);
		});
		connect(m_ui.btnCommandCancel, &QAbstractButton::clicked, &m_self, [this] {
			m_ui.stackedWidgetCommand->setCurrentWidget(m_ui.scriptCommandListPage);
		});
		connect(m_ui.btnCommandOk, &QAbstractButton::clicked, &m_self, [this] {
			OnCommandOkClicked();
		});
		connect(m_ui.lineEditCommandArguments, &QWidget::customContextMenuRequested, &m_self, [this] {
			IScriptController::ExecuteContextMenu(m_ui.lineEditCommandArguments);
		});
		connect(m_ui.actionOpenExe, &QAction::triggered, &m_self, [this] {
			OnOpenImpl(m_uiFactory->GetOpenFileName(DIALOG_KEY, Tr(FILE_DIALOG_TITLE), Tr(APP_FILE_FILTER)), *m_ui.lineEditCommandTextExe);
		});
		connect(m_ui.actionOpenCWD, &QAction::triggered, &m_self, [this] {
			OnOpenImpl(m_uiFactory->GetExistingDirectory(DIALOG_KEY, FOLDER_DIALOG_TITLE), *m_ui.lineEditCommandWorkingFolder);
		});
	}

	void OnAccept()
	{
		m_scriptModel->setData({}, {}, Qt::EditRole);
	}

	void SerializeCommand(const IScriptController::Command::Type type, const QString& command)
	{
		if (type != IScriptController::Command::Type::System)
			return;

		if (m_ui.comboBoxCommandTextSystem->findText(command) < 0)
			m_ui.comboBoxCommandTextSystem->addItem(command);

		QStringList commands;
		for (int i = 0, sz = m_ui.comboBoxCommandTextSystem->count(); i < sz; ++i)
			commands << m_ui.comboBoxCommandTextSystem->itemText(i);

		m_settings->Set(SYS_COMMAND_KEY, commands);
	}

	void OnEditCommandClicked() const
	{
		const auto commandTypeVar = m_ui.viewCommand->currentIndex().data(Role::Type);
		if (const auto index = m_ui.comboBoxCommandType->findData(commandTypeVar); index >= 0)
			m_ui.comboBoxCommandType->setCurrentIndex(index);

		const auto commandType = commandTypeVar.value<IScriptController::Command::Type>();
		FindSecond(COMMAND_GETTERS, commandType).second(m_commandTextWidgets, commandType, m_ui.viewCommand->currentIndex().data(Role::Command));

		m_ui.lineEditCommandArguments->setText(m_ui.viewCommand->currentIndex().data(Role::Arguments).toString());
		m_ui.lineEditCommandWorkingFolder->setText(m_ui.viewCommand->currentIndex().data(Role::WorkingFolder).toString());

		m_ui.stackedWidgetCommand->setCurrentWidget(m_ui.scriptCommandEditorPage);
	}

	void OnCommandOkClicked()
	{
		if (m_addMode)
		{
			m_commandModel->insertRow(m_commandModel->rowCount());
			m_ui.viewCommand->setCurrentIndex(m_commandModel->index(m_commandModel->rowCount() - 1, 0));
		}

		const auto [commandType, command] = [this] {
			const auto type    = m_ui.comboBoxCommandType->currentData().value<IScriptController::Command::Type>();
			const auto invoker = FindSecond(COMMAND_GETTERS, type).first;
			return std::pair(type, std::invoke(invoker, m_commandTextWidgets, type));
		}();

		SerializeCommand(commandType, command);

		m_commandModel->setData(m_ui.viewCommand->currentIndex(), m_ui.comboBoxCommandType->currentData(), Role::Type);
		m_commandModel->setData(m_ui.viewCommand->currentIndex(), command, Role::Command);
		m_commandModel->setData(m_ui.viewCommand->currentIndex(), m_ui.lineEditCommandArguments->text(), Role::Arguments);
		m_commandModel->setData(m_ui.viewCommand->currentIndex(), m_ui.lineEditCommandWorkingFolder->text(), Role::WorkingFolder);

		m_ui.stackedWidgetCommand->setCurrentWidget(m_ui.scriptCommandListPage);
	}

	void OnScriptSelectionChanged()
	{
		m_ui.stackedWidgetCommand->setCurrentWidget(m_ui.scriptCommandListPage);

		const auto selection = m_ui.viewScript->selectionModel()->selection().indexes();
		m_ui.btnRemoveScript->setEnabled(!selection.isEmpty());
		m_ui.btnAddCommand->setEnabled(selection.count() == 1);

		const auto currentIndex = m_ui.viewScript->currentIndex();
		m_ui.btnScriptUp->setEnabled(selection.count() == 1 && currentIndex.row() > 0);
		m_ui.btnScriptDown->setEnabled(selection.count() == 1 && currentIndex.isValid() && currentIndex.row() < m_ui.viewScript->model()->rowCount() - 1);
		m_commandModel->setData({}, currentIndex.isValid() ? currentIndex.data(Role::Uid) : QVariant {}, Role::Uid);
		m_ui.viewCommand->setCurrentIndex(m_commandModel->index(0, 1));
	}

	void OnComboBoxCommandTypeIndexChanged(const int index) const
	{
		for (auto* widget : m_commandTextWidgets)
			widget->setVisible(false);

		const auto widgetIndex = static_cast<size_t>(m_ui.comboBoxCommandType->itemData(index).value<IScriptController::Command::Type>());
		assert(widgetIndex < m_commandTextWidgets.size());
		m_commandTextWidgets[widgetIndex]->setVisible(true);
	}

	void OnCommandSelectionChanged() const
	{
		const auto selection = m_ui.viewCommand->selectionModel()->selection().indexes();
		m_ui.btnEditCommand->setEnabled(!selection.isEmpty());
		m_ui.btnRemoveCommand->setEnabled(!selection.isEmpty());

		const auto currentIndex = m_ui.viewCommand->currentIndex();
		m_ui.btnCommandUp->setEnabled(selection.count() == 1 && currentIndex.row() > 0);
		m_ui.btnCommandDown->setEnabled(selection.count() == 1 && currentIndex.isValid() && currentIndex.row() < m_ui.viewCommand->model()->rowCount() - 1);
	}

	void OnOpenImpl(QString path, QLineEdit& lineEdit) const
	{
		if (path.isEmpty())
			return;

		if (m_settings->Get(Constant::Settings::PREFER_RELATIVE_PATHS, false))
			path = Util::ToRelativePath(path);

		lineEdit.setText(QDir::toNativeSeparators(path));
	}

private:
	ScriptDialog&                                           m_self;
	std::shared_ptr<const Util::IUiFactory>                 m_uiFactory;
	PropagateConstPtr<ISettings, std::shared_ptr>           m_settings;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr>  m_scriptModel;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr>  m_commandModel;
	PropagateConstPtr<ComboBoxDelegate, std::shared_ptr>    m_scriptTypeDelegate;
	PropagateConstPtr<QStyledItemDelegate, std::shared_ptr> m_scriptNameLineEditDelegate;
	std::vector<QWidget*>                                   m_commandTextWidgets;
	bool                                                    m_addMode { false };
	Ui::ScriptDialog                                        m_ui {};
};

ScriptDialog::ScriptDialog(
	const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
	const std::shared_ptr<const IModelProvider>&  modelProvider,
	std::shared_ptr<const Util::IUiFactory>       uiFactory,
	std::shared_ptr<ISettings>                    settings,
	std::shared_ptr<ScriptComboBoxDelegate>       scriptTypeDelegate,
	std::shared_ptr<ScriptNameDelegate>           scriptNameLineEditDelegate
)
	: QDialog(parentWidgetProvider->GetWidget())
	, m_impl(*this, *modelProvider, std::move(uiFactory), std::move(settings), std::move(scriptTypeDelegate), std::move(scriptNameLineEditDelegate))
{
	PLOGV << "ScriptDialog created";
}

ScriptDialog::~ScriptDialog()
{
	PLOGV << "ScriptDialog destroyed";
}

int ScriptDialog::Exec()
{
	return QDialog::exec();
}
