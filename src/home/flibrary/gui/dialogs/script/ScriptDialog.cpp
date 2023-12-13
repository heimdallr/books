#include "ui_ScriptDialog.h"
#include "ScriptDialog.h"

#include <ranges>
#include <set>

#include <plog/Log.h>

#include "fnd/algorithm.h"
#include "interface/constants/Localization.h"
#include "interface/logic/IModelProvider.h"
#include "interface/logic/IScriptController.h"

#include "ComboBoxDelegate.h"
#include "CommonLineEditDelegate.h"
#include "GeometryRestorable.h"
#include "ParentWidgetProvider.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

using Role = IScriptController::RoleBase;

constexpr auto COLUMN_WIDTH_KEY_TEMPLATE = "ui/%1/%2/Column%3Width";

void RemoveRows(const QAbstractItemView& view)
{
	std::set<int> rows;
	std::ranges::transform(view.selectionModel()->selection().indexes(), std::inserter(rows, rows.end()), [] (const QModelIndex & index) { return index.row(); });
	for (const auto & [begin, end] : Util::CreateRanges(rows) | std::views::reverse)
		view.model()->removeRows(begin, end - begin);
}

void SaveLayout(const QObject & parent, const QTableView & view, ISettings & settings)
{
	for (int i = 0, sz = view.horizontalHeader()->count() - 1; i < sz; ++i)
		settings.Set(QString(COLUMN_WIDTH_KEY_TEMPLATE).arg(parent.objectName()).arg(view.objectName()).arg(i), view.horizontalHeader()->sectionSize(i));
}

void LoadLayout(const QObject & parent, const QTableView & view, const ISettings & settings)
{
	for (int i = 0, sz = view.horizontalHeader()->count() - 1; i < sz; ++i)
		if (const auto width = settings.Get(QString(COLUMN_WIDTH_KEY_TEMPLATE).arg(parent.objectName()).arg(view.objectName()).arg(i)); width.isValid())
			view.horizontalHeader()->resizeSection(i, width.toInt());
}

template <std::ranges::forward_range R, class Proj = std::identity>
void SetupView(const QObject & parent, ISettings & settings, QTableView & view, QAbstractItemModel & model, IComboBoxDelegate & delegate, R && array, Proj proj = {})
{
	view.setModel(&model);
	view.setItemDelegateForColumn(0, &delegate);
	IComboBoxDelegate::Values types;
	types.reserve(std::size(array));
	std::ranges::transform(array, std::back_inserter(types), [&] (const auto & item)
	{
		return std::make_pair(static_cast<int>(item.first), Loc::Tr(IScriptController::s_context, std::invoke(proj, item.second)));
	});
	delegate.SetValues(std::move(types));

	LoadLayout(parent, view, settings);
}

}

class ScriptDialog::Impl final
	: GeometryRestorable
	, GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(ScriptDialog& self
		, const IModelProvider & modelProvider
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<IComboBoxDelegate> scriptTypeDelegate
		, std::shared_ptr<IComboBoxDelegate> commandTypeDelegate
		, std::shared_ptr<QStyledItemDelegate> scriptNameLineEditDelegate
		, std::shared_ptr<QStyledItemDelegate> commandArgLineEditDelegate
	)
		: GeometryRestorable(*this, settings, "ScriptDialog")
		, GeometryRestorableObserver(self)
		, m_self(self)
		, m_settings(std::move(settings))
		, m_scriptModel(modelProvider.CreateScriptModel())
		, m_commandModel(modelProvider.CreateScriptCommandModel())
		, m_scriptTypeDelegate(std::move(scriptTypeDelegate))
		, m_commandTypeDelegate(std::move(commandTypeDelegate))
		, m_scriptNameLineEditDelegate(std::move(scriptNameLineEditDelegate))
		, m_commandArgLineEditDelegate(std::move(commandArgLineEditDelegate))
	{
		m_ui.setupUi(&m_self);

		SetupView(m_self, *m_settings, *m_ui.viewScript, *m_scriptModel, *m_scriptTypeDelegate, IScriptController::s_scriptTypes);
		SetupView(m_self, *m_settings, *m_ui.viewCommand, *m_commandModel, *m_commandTypeDelegate, IScriptController::s_commandTypes, &IScriptController::CommandDescription::type);

		m_ui.viewScript->setItemDelegateForColumn(1, m_scriptNameLineEditDelegate.get());
		m_ui.viewCommand->setItemDelegateForColumn(2, m_commandArgLineEditDelegate.get());

		m_ui.viewCommand->setModel(m_commandModel.get());

		SetConnections();

		const auto headerColor = m_ui.viewScript->palette().color(QPalette::Base);
		const auto style = QString("QHeaderView::section { background-color: rgb(%1, %2, %3) }").arg(headerColor.red()).arg(headerColor.green()).arg(headerColor.blue());
		m_ui.viewScript->setStyleSheet(style);
		m_ui.viewCommand->setStyleSheet(style);

		m_ui.viewScript->setCurrentIndex(m_scriptModel->index(0, 1));

		Init();
	}

	~Impl() override
	{
		SaveLayout(m_self, *m_ui.viewScript, *m_settings);
		SaveLayout(m_self, *m_ui.viewCommand, *m_settings);
	}

private:
	void SetConnections()
	{
		connect(m_ui.viewScript->selectionModel(), &QItemSelectionModel::selectionChanged, &m_self, [&]
		{
			OnScriptSelectionChanged();
		});
		connect(m_scriptModel.get(), &QAbstractItemModel::modelReset, &m_self, [&]
		{
			OnScriptSelectionChanged();
		});
		connect(m_ui.btnAddScript, &QAbstractButton::clicked, &m_self, [&]
		{
			m_scriptModel->insertRow(m_scriptModel->rowCount());
			m_ui.viewScript->setCurrentIndex(m_scriptModel->index(m_scriptModel->rowCount() - 1, 1));
		});
		connect(m_ui.btnRemoveScript, &QAbstractButton::clicked, &m_self, [&]
		{
			RemoveRows(*m_ui.viewScript);
		});
		connect(m_ui.btnScriptUp, &QAbstractButton::clicked, &m_self, [&]
		{
			m_scriptModel->setData(m_ui.viewScript->currentIndex(), {}, Role::Up);
			OnScriptSelectionChanged();
		});
		connect(m_ui.btnScriptDown, &QAbstractButton::clicked, &m_self, [&]
		{
			m_scriptModel->setData(m_ui.viewScript->currentIndex(), {}, Role::Down);
			OnScriptSelectionChanged();
		});

		connect(m_ui.viewCommand->selectionModel(), &QItemSelectionModel::selectionChanged, &m_self, [&]
		{
			OnCommandSelectionChanged();
		});
		connect(m_commandModel.get(), &QAbstractItemModel::modelReset, &m_self, [&]
		{
			OnCommandSelectionChanged();
		});
		connect(m_ui.btnAddCommand, &QAbstractButton::clicked, &m_self, [&]
		{
			m_commandModel->insertRow(m_commandModel->rowCount());
			m_ui.viewCommand->setCurrentIndex(m_commandModel->index(m_commandModel->rowCount() - 1, 1));
		});
		connect(m_ui.btnRemoveCommand, &QAbstractButton::clicked, &m_self, [&]
		{
			RemoveRows(*m_ui.viewCommand);
		});
		connect(m_ui.btnCommandUp, &QAbstractButton::clicked, &m_self, [&]
		{
			m_commandModel->setData(m_ui.viewCommand->currentIndex(), {}, Role::Up);
			OnCommandSelectionChanged();
		});
		connect(m_ui.btnCommandDown, &QAbstractButton::clicked, &m_self, [&]
		{
			m_commandModel->setData(m_ui.viewCommand->currentIndex(), {}, Role::Down);
			OnCommandSelectionChanged();
		});

		connect(m_ui.btnSave, &QAbstractButton::clicked, &m_self, [&]
		{
			m_scriptModel->setData({}, {}, Qt::EditRole); m_self.accept();
		});
		connect(m_ui.btnCancel, &QAbstractButton::clicked, &m_self, &QDialog::reject);
	}

	void OnScriptSelectionChanged()
	{
		const auto selection = m_ui.viewScript->selectionModel()->selection().indexes();
		m_ui.btnRemoveScript->setEnabled(!selection.isEmpty());
		m_ui.btnAddCommand->setEnabled(selection.count() == 1);
		m_ui.btnScriptUp->setEnabled(selection.count() == 1 && m_ui.viewScript->currentIndex().row() > 0);
		m_ui.btnScriptDown->setEnabled(selection.count() == 1 && m_ui.viewScript->currentIndex().isValid() && m_ui.viewScript->currentIndex().row() < m_ui.viewScript->model()->rowCount() - 1);
		m_commandModel->setData({}, m_ui.viewScript->currentIndex().isValid() ? m_ui.viewScript->currentIndex().data(Role::Uid) : QVariant {}, Role::Uid);
		m_ui.viewCommand->setCurrentIndex(m_commandModel->index(0, 1));
	}

	void OnCommandSelectionChanged() const
	{
		const auto selection = m_ui.viewCommand->selectionModel()->selection().indexes();
		m_ui.btnRemoveCommand->setEnabled(!selection.isEmpty());
		m_ui.btnCommandUp->setEnabled(selection.count() == 1 && m_ui.viewCommand->currentIndex().row() > 0);
		m_ui.btnCommandDown->setEnabled(selection.count() == 1 && m_ui.viewCommand->currentIndex().isValid() && m_ui.viewCommand->currentIndex().row() < m_ui.viewCommand->model()->rowCount() - 1);
	}

private:
	ScriptDialog & m_self;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_scriptModel;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_commandModel;
	PropagateConstPtr<IComboBoxDelegate, std::shared_ptr> m_scriptTypeDelegate;
	PropagateConstPtr<IComboBoxDelegate, std::shared_ptr> m_commandTypeDelegate;
	PropagateConstPtr<QStyledItemDelegate, std::shared_ptr> m_scriptNameLineEditDelegate;
	PropagateConstPtr<QStyledItemDelegate, std::shared_ptr> m_commandArgLineEditDelegate;
	Ui::ScriptDialog m_ui{};
};

ScriptDialog::ScriptDialog(const std::shared_ptr<ParentWidgetProvider> & parentWidgetProvider
	, const std::shared_ptr<const IModelProvider> & modelProvider
	, std::shared_ptr<ISettings> settings
	, std::shared_ptr<ScriptComboBoxDelegate> scriptTypeDelegate
	, std::shared_ptr<CommandComboBoxDelegate> commandTypeDelegate
	, std::shared_ptr<ScriptNameLineEditDelegate> scriptNameLineEditDelegate
	, std::shared_ptr<CommandArgLineEditDelegate> commandArgLineEditDelegate
)
	: QDialog(parentWidgetProvider->GetWidget())
	, m_impl(*this
		, *modelProvider
		, std::move(settings)
		, std::move(scriptTypeDelegate)
		, std::move(commandTypeDelegate)
		, std::move(scriptNameLineEditDelegate)
		, std::move(commandArgLineEditDelegate)
	)
{
	PLOGD << "ScriptDialog created";
}

ScriptDialog::~ScriptDialog()
{
	PLOGD << "ScriptDialog destroyed";
}

int ScriptDialog::Exec()
{
	return QDialog::exec();
}
