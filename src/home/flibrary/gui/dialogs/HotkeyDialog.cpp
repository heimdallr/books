#include "ui_HotkeyDialog.h"

#include "HotkeyDialog.h"

#include <QIdentityProxyModel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolTip>

#include "fnd/IsOneOf.h"

#include "interface/constants/ModelRole.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"

#include "gutil/util.h"
#include "logic/data/DataItem.h"
#include "utilgui/GeometryRestorable.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto CONTEXT            = "HotkeyDialog";
constexpr auto RESET              = QT_TRANSLATE_NOOP("HotkeyDialog", "Remove hotkey");
constexpr auto SET_ICON           = QT_TRANSLATE_NOOP("HotkeyDialog", "Set icon");
constexpr auto REMOVE_ICON        = QT_TRANSLATE_NOOP("HotkeyDialog", "Remove icon");
constexpr auto ALREADY_USED       = QT_TRANSLATE_NOOP("HotkeyDialog", "%1 already in use:\n%2");
constexpr auto SELECT_ICON        = QT_TRANSLATE_NOOP("HotkeyDialog", "Select image file");
constexpr auto SELECT_ICON_FILTER = QT_TRANSLATE_NOOP("HotkeyDialog", "Images (*.ico *.png *.bmp *.jpg *.jpeg);;All files (*.*)");

TR_DEF

constexpr auto ICONS           = "HotkeyDialogIcons";
constexpr auto FIELD_WIDTH_KEY = "ui/View/HotkeyDialog/columnWidths";

class Model final : public QIdentityProxyModel
{
public:
	struct ModelRole
	{
		enum
		{
			Key = Role::Last,
			AlreadyAssigned,
			Icon,
			Last
		};
	};

public:
	static std::unique_ptr<QAbstractItemModel> Create(const IModelProvider& modelProvider, std::shared_ptr<IHotkeyManager> hotkeyManager)
	{
		return std::make_unique<Model>(modelProvider, std::move(hotkeyManager));
	}

	Model(const IModelProvider& modelProvider, std::shared_ptr<IHotkeyManager> hotkeyManager, QObject* parent = nullptr)
		: QIdentityProxyModel(parent)
		, m_hotkeyManager { std::move(hotkeyManager) }
		, m_source { modelProvider.CreateTreeModel(m_hotkeyManager->GetRootDataItem()) }
	{
		QIdentityProxyModel::setSourceModel(m_source.get());
	}

private: // QAbstractItemModel
	[[nodiscard]] int columnCount(const QModelIndex&) const override
	{
		return 2;
	}

	QVariant data(const QModelIndex& index, const int role) const override
	{
		if (index.isValid())
		{
			if (index.column() == 0)
			{
				const auto sourceIndex = mapToSource(index);
				switch (role)
				{
					case Qt::DisplayRole:
					case Qt::ToolTipRole:
						return m_source->index(sourceIndex.row(), SettingsItem::Column::Title, sourceIndex.parent()).data(role);

					case Qt::DecorationRole:
						return m_hotkeyManager->GetIcon(m_source->index(sourceIndex.row(), SettingsItem::Column::Key, sourceIndex.parent()).data().toString());

					case ModelRole::Key:
						return m_source->index(sourceIndex.row(), SettingsItem::Column::Key, sourceIndex.parent()).data();

					default:
						break;
				}
			}
		}
		else
		{
			if (role == ModelRole::AlreadyAssigned)
				return m_alreadyAssigned;
		}

		return QIdentityProxyModel::data(index, role);
	}

	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		if (!index.isValid())
			return false;

		QVector<int> roles;

		switch (role)
		{
			case Qt::EditRole:
				if (SetShortCut(index, value))
					roles.emplace_back(Qt::DisplayRole);
				break;

			case ModelRole::Icon:
				roles.emplace_back(Qt::DecorationRole);
				break;

			default:
				break;
		}

		if (!roles.empty())
			emit dataChanged(index, index, roles);

		return !roles.empty();
	}

	Qt::ItemFlags flags(const QModelIndex& index) const override
	{
		return QIdentityProxyModel::flags(index) | (index.column() == 1 && index.data(Role::ChildCount).toInt() == 0 ? Qt::ItemIsEditable : Qt::NoItemFlags);
	}

private:
	bool SetShortCut(const QModelIndex& index, const QVariant& value)
	{
		const auto shortCut = value.toString();
		if (shortCut.isEmpty())
		{
			m_hotkeyManager->Reset(GetKey(index));
			return true;
		}

		if (auto alreadyAssigned = m_hotkeyManager->Set(GetKey(index), shortCut); !alreadyAssigned.isEmpty())
			return (m_alreadyAssigned = std::move(alreadyAssigned)), false;

		return true;
	}

	QString GetKey(const QModelIndex& index) const
	{
		const auto sourceIndex = mapToSource(index);
		return m_source->index(sourceIndex.row(), SettingsItem::Column::Key, sourceIndex.parent()).data().toString();
	}

private:
	PropagateConstPtr<IHotkeyManager, std::shared_ptr>     m_hotkeyManager;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_source;

	QString m_alreadyAssigned;
};

class HotkeyDelegate final : public QStyledItemDelegate
{
public:
	explicit HotkeyDelegate(QObject* parent = nullptr)
		: QStyledItemDelegate(parent)
	{
	}

private: // QStyledItemDelegate
	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override
	{
		m_editor = new QLineEdit(parent);
		m_editor->installEventFilter(const_cast<HotkeyDelegate*>(this));
		m_editor->setReadOnly(true);
		return m_editor;
	}

	void setEditorData(QWidget* editor, const QModelIndex& index) const override
	{
		auto* lineEdit = qobject_cast<QLineEdit*>(editor);
		lineEdit->setText(index.data().toString());
	}

	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
	{
		auto text = qobject_cast<QLineEdit*>(editor)->text();
		if (model->setData(index, text))
			return;

		QTimer::singleShot(0, [model, font = editor->font(), text = std::move(text)] {
			QToolTip::setFont(font);
			QToolTip::showText(QCursor::pos(), Tr(ALREADY_USED).arg(text, model->data({}, Model::ModelRole::AlreadyAssigned).toString()));
		});
	}

private: // QObject
	bool eventFilter(QObject*, QEvent* event) override
	{
		if (!IsOneOf(event->type(), QEvent::KeyPress, QEvent::KeyRelease))
			return false;

		if (!m_editor)
			return false;

		if (event->type() == QEvent::KeyRelease)
		{
			emit commitData(m_editor);
			emit closeEditor(m_editor, NoHint);
			return true;
		}

		const auto*        keyEvent = static_cast<const QKeyEvent*>(event);
		const QKeySequence keySequence =
			(IsOneOf(keyEvent->key(), Qt::Key_Alt, Qt::Key_Control, Qt::Key_Shift) ? 0 : keyEvent->key()) | (keyEvent->modifiers() & (Qt::AltModifier | Qt::ControlModifier | Qt::ShiftModifier));

		m_editor->setText(keySequence.toString());

		return true;
	}

private:
	mutable QPointer<QLineEdit> m_editor;
};

} // namespace

class HotkeyDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		QDialog&                                   self,
		const IModelProvider&                      modelProvider,
		std::shared_ptr<const Util::IUiFactory>    uiFactory,
		std::shared_ptr<ISettings>                 settings,
		std::shared_ptr<IHotkeyManager>            hotkeyManager,
		std::shared_ptr<Util::ItemViewToolTipper>  itemViewToolTipper,
		std::shared_ptr<Util::ScrollBarController> scrollBarController
	)
		: GeometryRestorable(*this, settings, CONTEXT)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_uiFactory { std::move(uiFactory) }
		, m_settings { std::move(settings) }
		, m_hotkeyManager { std::move(hotkeyManager) }
		, m_model { Model::Create(modelProvider, m_hotkeyManager) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
	{
		m_ui.setupUi(&m_self);

		m_itemViewToolTipper->SetScrollArea(m_ui.view);
		m_scrollBarController->SetScrollArea(m_ui.view);

		m_ui.view->setModel(m_model.get());
		m_ui.view->setAlternatingRowColors(m_settings->Get(Constant::Settings::PREFER_ALTERNATING_ROW_COLORS, false));
		auto& header = *m_ui.view->header();
		header.setDefaultAlignment(Qt::AlignCenter);
		header.setSectionResizeMode(1, QHeaderView::ResizeToContents);
		header.setSectionResizeMode(0, QHeaderView::Stretch);
		m_ui.view->setItemDelegateForColumn(1, new HotkeyDelegate(&m_self));
		m_ui.view->setCurrentIndex({});
		for (int i = 0, sz = m_model->rowCount(); i < sz; ++i)
			m_ui.view->expand(m_model->index(i, 0));

		connect(m_ui.view, &QWidget::customContextMenuRequested, &self, [this] {
			CreateContextMenu();
		});

		LoadGeometry();
		Util::LoadHeaderSectionWidth(*m_ui.view->header(), *m_settings, FIELD_WIDTH_KEY);
	}

	~Impl() override
	{
		Util::SaveHeaderSectionWidth(*m_ui.view->header(), *m_settings, FIELD_WIDTH_KEY);
		SaveGeometry();
	}

private:
	void CreateContextMenu()
	{
		QMenu menu;
		menu.setFont(m_self.font());

		if (m_ui.view->currentIndex().data(Role::ChildCount).toInt() == 0)
			menu.addAction(Tr(RESET), [this] {
				m_model->setData(m_ui.view->currentIndex(), {});
			});
		if (m_ui.view->currentIndex().parent().isValid())
		{
			menu.addAction(Tr(SET_ICON), [this] {
				if (const auto path = m_uiFactory->GetOpenFileName(ICONS, Tr(SELECT_ICON), Tr(SELECT_ICON_FILTER)); !path.isEmpty())
				{
					if (const auto result = m_hotkeyManager->SetIcon(m_ui.view->currentIndex().data(Model::ModelRole::Key).toString(), path); !result)
						m_uiFactory->ShowError(result.error());
					else
						m_model->setData(m_ui.view->currentIndex(), {}, Model::ModelRole::Icon);
				}
			});

			if (!m_model->data(m_ui.view->currentIndex(), Qt::DecorationRole).value<QIcon>().isNull())
				menu.addAction(Tr(REMOVE_ICON), [this] {
					(void)m_hotkeyManager->SetIcon(m_ui.view->currentIndex().data(Model::ModelRole::Key).toString());
					m_model->setData(m_ui.view->currentIndex(), {}, Model::ModelRole::Icon);
				});
		}
		Util::FillTreeContextMenu(*m_ui.view, menu).exec(QCursor::pos());
	}

private:
	QDialog& m_self;

	std::shared_ptr<const Util::IUiFactory>                       m_uiFactory;
	PropagateConstPtr<ISettings, std::shared_ptr>                 m_settings;
	PropagateConstPtr<IHotkeyManager, std::shared_ptr>            m_hotkeyManager;
	PropagateConstPtr<QAbstractItemModel>                         m_model;
	PropagateConstPtr<Util::ItemViewToolTipper, std::shared_ptr>  m_itemViewToolTipper;
	PropagateConstPtr<Util::ScrollBarController, std::shared_ptr> m_scrollBarController;

	Ui::HotkeyDialog m_ui;
};

HotkeyDialog::HotkeyDialog(
	const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
	const std::shared_ptr<IModelProvider>&        modelProvider,
	std::shared_ptr<const Util::IUiFactory>       uiFactory,
	std::shared_ptr<ISettings>                    settings,
	std::shared_ptr<IHotkeyManager>               hotkeyManager,
	std::shared_ptr<Util::ItemViewToolTipper>     itemViewToolTipper,
	std::shared_ptr<Util::ScrollBarController>    scrollBarController,
	QWidget*                                      parent
)
	: QDialog(parentWidgetProvider->GetWidget(parent))
	, m_impl(*this, *modelProvider, std::move(uiFactory), std::move(settings), std::move(hotkeyManager), std::move(itemViewToolTipper), std::move(scrollBarController))
{
}

HotkeyDialog::~HotkeyDialog() = default;
