#include "ui_ScriptDialog.h"
#include "ScriptDialog.h"

#include <ranges>
#include <set>

#include <plog/Log.h>

#include "fnd/algorithm.h"
#include "interface/constants/Localization.h"
#include "interface/logic/IModelProvider.h"
#include "interface/logic/IScriptController.h"

#include "GeometryRestorable.h"
#include "ParentWidgetProvider.h"
#include "ComboBoxDelegate.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

using Role = IScriptController::Role;

void RemoveRows(const QAbstractItemView& view)
{
	std::set<int> rows;
	std::ranges::transform(view.selectionModel()->selection().indexes(), std::inserter(rows, rows.end()), [] (const QModelIndex & index)
	{
		return index.row();
	});
	for (const auto & [begin, end] : Util::CreateRanges(rows) | std::views::reverse)
		view.model()->removeRows(begin, end - begin);
}
}

class ScriptDialog::Impl final
	: GeometryRestorable
	, GeometryRestorableObserver
{
public:
	Impl(ScriptDialog& self
		, const IModelProvider & modelProvider
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<IComboBoxDelegate> scriptDelegate
	)
		: GeometryRestorable(*this, std::move(settings), "ScriptDialog")
		, GeometryRestorableObserver(self)
		, m_self(self)
		, m_scriptModel(modelProvider.CreateScriptModel())
		, m_commandModel(modelProvider.CreateScriptCommandModel())
		, m_scriptDelegate(std::move(scriptDelegate))
	{
		m_ui.setupUi(&m_self);

		m_ui.viewScript->setModel(m_scriptModel.get());
		m_ui.viewScript->setItemDelegateForColumn(0, m_scriptDelegate.get());
		IComboBoxDelegate::Values scriptTypes;
		scriptTypes.reserve(std::size(IScriptController::s_scriptTypes));
		std::ranges::transform(IScriptController::s_scriptTypes, std::back_inserter(scriptTypes), [] (const auto & item)
		{
			return std::make_pair(static_cast<int>(item.first), Loc::Tr(IScriptController::s_context, item.second));
		});
		m_scriptDelegate->SetValues(std::move(scriptTypes));

		m_ui.viewCommand->setModel(m_commandModel.get());

		SetConnections();

		const auto headerColor = m_ui.viewScript->palette().color(QPalette::Base);
		const auto style = QString("QHeaderView::section { background-color: rgb(%1, %2, %3) }").arg(headerColor.red()).arg(headerColor.green()).arg(headerColor.blue());
		m_ui.viewScript->setStyleSheet(style);
		m_ui.viewCommand->setStyleSheet(style);

		Init();
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
			m_commandModel->insertRow(m_scriptModel->rowCount());
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

	void OnScriptSelectionChanged() const
	{
		const auto selection = m_ui.viewScript->selectionModel()->selection();
		m_ui.btnRemoveScript->setEnabled(!selection.isEmpty());
		m_ui.btnAddCommand->setEnabled(selection.count() == 1);
		m_ui.btnScriptUp->setEnabled(m_ui.viewScript->currentIndex().row() > 0);
		m_ui.btnScriptDown->setEnabled(m_ui.viewScript->currentIndex().isValid() && m_ui.viewScript->currentIndex().row() < m_ui.viewScript->model()->rowCount() - 1);
	}

	void OnCommandSelectionChanged() const
	{
		const auto selection = m_ui.viewCommand->selectionModel()->selection();
		m_ui.btnRemoveCommand->setEnabled(!selection.isEmpty());
		m_ui.btnCommandUp->setEnabled(m_ui.viewCommand->currentIndex().row() > 0);
		m_ui.btnCommandDown->setEnabled(m_ui.viewCommand->currentIndex().isValid() && m_ui.viewCommand->currentIndex().row() < m_ui.viewCommand->model()->rowCount() - 1);
	}

private:
	ScriptDialog & m_self;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_scriptModel;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_commandModel;
	PropagateConstPtr<IComboBoxDelegate, std::shared_ptr> m_scriptDelegate;
	Ui::ScriptDialog m_ui{};
};

ScriptDialog::ScriptDialog(const std::shared_ptr<ParentWidgetProvider> & parentWidgetProvider
	, const std::shared_ptr<const IModelProvider> & modelProvider
	, std::shared_ptr<ISettings> settings
	, std::shared_ptr<ScriptComboBoxDelegate> scriptDelegate
)
	: QDialog(parentWidgetProvider->GetWidget())
	, m_impl(*this
		, *modelProvider
		, std::move(settings)
		, std::move(scriptDelegate)
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
