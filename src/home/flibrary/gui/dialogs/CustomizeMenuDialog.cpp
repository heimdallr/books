#include "ui_CustomizeMenuDialog.h"

#include "CustomizeMenuDialog.h"

#include <ranges>

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

constexpr auto CONTEXT              = "HotkeyDialog";
constexpr auto REMOVE_HOTKEY        = QT_TRANSLATE_NOOP("HotkeyDialog", "Remove hotkey");
constexpr auto REMOVE_ICON          = QT_TRANSLATE_NOOP("HotkeyDialog", "Remove icon");
constexpr auto SELECT_ICON          = QT_TRANSLATE_NOOP("HotkeyDialog", "Select image file");
constexpr auto SELECT_ICON_FILTER   = QT_TRANSLATE_NOOP("HotkeyDialog", "Image files (*.ico *.png *.bmp *.jpg *.jpeg);;All files (*.*)");
constexpr auto SET_ICON             = QT_TRANSLATE_NOOP("HotkeyDialog", "Set icon");
constexpr auto SET_HOTKEY           = QT_TRANSLATE_NOOP("HotkeyDialog", "Set hotkey");
constexpr auto TOOLTIP_ALREADY_USED = QT_TRANSLATE_NOOP("HotkeyDialog", "%1 already in use:\n%2");
constexpr auto TOOLTIP_HIDE         = QT_TRANSLATE_NOOP("HotkeyDialog", "Check to hide \"%1\"");
constexpr auto TOOLTIP_ITEM_ICON    = QT_TRANSLATE_NOOP("HotkeyDialog", "Icon for %1");
constexpr auto TOOLTIP_SET_HOTKEY   = QT_TRANSLATE_NOOP("HotkeyDialog", "Double-click to set the hotkey for \"%1\"");

TR_DEF

constexpr auto ICONS = "HotkeyDialogIcons";

struct Column
{
	enum
	{
		Title,
		Hotkey,
		Icon,
		Hidden,
		Last
	};
};

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
			Last [[maybe_unused]]
		};
	};

public:
	static std::unique_ptr<QAbstractItemModel> Create(const IModelProvider& modelProvider, std::shared_ptr<IMenuCustomizer> menuCustomizer)
	{
		return std::make_unique<Model>(modelProvider, std::move(menuCustomizer));
	}

	Model(const IModelProvider& modelProvider, std::shared_ptr<IMenuCustomizer> menuCustomizer, QObject* parent = nullptr)
		: QIdentityProxyModel(parent)
		, m_menuCustomizer { std::move(menuCustomizer) }
		, m_source { modelProvider.CreateTreeModel(m_menuCustomizer->GetRootDataItem()) }
	{
		m_menuCustomizer->UpdateItems();
		m_source->setData({}, Column::Last, Role::ColumnCount);
		QIdentityProxyModel::setSourceModel(m_source.get());
	}

private: // QAbstractItemModel
	QVariant data(const QModelIndex& index, const int role) const override
	{
		return index.isValid() ? GetData(index, role) : GetData(role);
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
					roles.push_back(Qt::DisplayRole);
				break;

			case Qt::CheckStateRole:
			{
				const auto sourceIndex = mapToSource(index);
				m_menuCustomizer->Hide(m_source->index(sourceIndex.row(), SettingsItem::Column::Key, sourceIndex.parent()).data().toString(), value.value<Qt::CheckState>() == Qt::Checked);
				roles.push_back(Qt::CheckStateRole);
				const auto titleIndex = this->index(index.row(), Column::Title, index.parent());
				emit       dataChanged(titleIndex, titleIndex, { Qt::ForegroundRole, Qt::FontRole });
				break;
			}

			case ModelRole::Icon:
				roles.push_back(Qt::DecorationRole);
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
		return QIdentityProxyModel::flags(index) | GetItemFlags(index);
	}

private:
	QVariant GetData(const int role) const
	{
		if (role == ModelRole::AlreadyAssigned)
			return m_alreadyAssigned;

		return {};
	}

	QVariant GetData(const QModelIndex& index, const int role) const
	{
		const auto sourceIndex = mapToSource(index);
		const auto key         = m_source->index(sourceIndex.row(), SettingsItem::Column::Key, sourceIndex.parent()).data().toString();
		if (role == ModelRole::Key)
		{
			return key;
		}
		switch (index.column())
		{
			case Column::Title:
				switch (role)
				{
					case Qt::DisplayRole:
					case Qt::ToolTipRole:
						return m_source->index(sourceIndex.row(), SettingsItem::Column::Title, sourceIndex.parent()).data(role);

					case Qt::ForegroundRole:
						if (m_menuCustomizer->IsHidden(key).toBool())
							return QColor(Qt::gray);
						break;

					case Qt::FontRole:
						if (m_menuCustomizer->IsHidden(key).toBool())
						{
							auto font = QIdentityProxyModel::data(index, Qt::FontRole).value<QFont>();
							font.setItalic(true);
							return QVariant::fromValue(font);
						}
						break;

					default:
						break;
				}
				break;

			case Column::Hotkey:
				if (role == Qt::ToolTipRole && !!(m_menuCustomizer->GetAbilities(key) & IMenuCustomizer::ItemAbility::Hotkey))
					return Tr(TOOLTIP_SET_HOTKEY).arg(m_source->index(sourceIndex.row(), SettingsItem::Column::Title, sourceIndex.parent()).data().toString());
				break;

			case Column::Icon:
				switch (role)
				{
					case Qt::DecorationRole:
						return m_menuCustomizer->GetIcon(key);

					case Qt::ToolTipRole:
						if (m_menuCustomizer->GetIcon(key).isValid())
							return Tr(TOOLTIP_ITEM_ICON).arg(m_source->index(sourceIndex.row(), SettingsItem::Column::Title, sourceIndex.parent()).data().toString());
						break;

					default:
						break;
				}
				return {};

			case Column::Hidden:
				switch (role)
				{
					case Qt::ToolTipRole:
						if (!!(m_menuCustomizer->GetAbilities(key) & IMenuCustomizer::ItemAbility::Hide))
							return Tr(TOOLTIP_HIDE).arg(m_source->index(sourceIndex.row(), SettingsItem::Column::Title, sourceIndex.parent()).data().toString());
						break;

					case Qt::CheckStateRole:
						if (!(m_menuCustomizer->GetAbilities(key) & IMenuCustomizer::ItemAbility::Hide))
							return {};
						if (const auto hidden = m_menuCustomizer->IsHidden(key); hidden.isValid())
							return hidden.toBool() ? Qt::Checked : Qt::Unchecked;
						return Qt::Unchecked;

					default:
						break;
				}
				return {};

			default:
				assert(false && "unexpected column");
		}

		return QIdentityProxyModel::data(index, role);
	}

	Qt::ItemFlags GetItemFlags(const QModelIndex& index) const
	{
		Qt::ItemFlags flags = Qt::NoItemFlags;
		if (IsOneOf(Column::Title, Column::Icon))
			return flags;

		const auto key       = index.data(ModelRole::Key).toString();
		const auto abilities = m_menuCustomizer->GetAbilities(key);

		if (index.column() == Column::Hotkey && !!(abilities & IMenuCustomizer::ItemAbility::Hotkey))
			flags |= Qt::ItemIsEditable;

		if (index.column() == Column::Hidden && !!(abilities & IMenuCustomizer::ItemAbility::Hide))
			flags |= Qt::ItemIsEditable | Qt::ItemIsUserCheckable;

		return flags;
	}

	bool SetShortCut(const QModelIndex& index, const QVariant& value)
	{
		const auto shortCut = value.toString();
		if (shortCut.isEmpty())
		{
			m_menuCustomizer->SetHotkey(GetKey(index));
			return true;
		}

		if (auto alreadyAssigned = m_menuCustomizer->SetHotkey(GetKey(index), shortCut); !alreadyAssigned.isEmpty())
			return (m_alreadyAssigned = std::move(alreadyAssigned)), false;

		return true;
	}

	QString GetKey(const QModelIndex& index) const
	{
		const auto sourceIndex = mapToSource(index);
		return m_source->index(sourceIndex.row(), SettingsItem::Column::Key, sourceIndex.parent()).data().toString();
	}

private:
	PropagateConstPtr<IMenuCustomizer, std::shared_ptr>    m_menuCustomizer;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_source;

	QString m_alreadyAssigned;
};

/*
class IconCenterDelegate final : public QStyledItemDelegate
{
public:
	explicit IconCenterDelegate(QObject* parent = nullptr)
		: QStyledItemDelegate(parent)
	{
	}

private: // QStyledItemDelegate
	void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
	{
		QStyledItemDelegate::initStyleOption(option, index);
		option->decorationAlignment = Qt::AlignCenter;
		option->decorationSize      = option->rect.size();
	}

	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		QStyledItemDelegate::paint(painter, option, index);
	}
};
*/
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
			QToolTip::showText(QCursor::pos(), Tr(TOOLTIP_ALREADY_USED).arg(text, model->data({}, Model::ModelRole::AlreadyAssigned).toString()));
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

class CustomizeMenuDialog::Impl final
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
		std::shared_ptr<IMenuCustomizer>           menuCustomizer,
		std::shared_ptr<Util::ItemViewToolTipper>  itemViewToolTipper,
		std::shared_ptr<Util::ScrollBarController> scrollBarController
	)
		: GeometryRestorable(*this, settings, CONTEXT)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_uiFactory { std::move(uiFactory) }
		, m_settings { std::move(settings) }
		, m_menuCustomizer { std::move(menuCustomizer) }
		, m_model { Model::Create(modelProvider, m_menuCustomizer) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
	{
		m_ui.setupUi(&m_self);

		m_itemViewToolTipper->SetShowForceColumns({ Column::Hotkey, Column::Icon, Column::Hidden });
		m_itemViewToolTipper->SetScrollArea(m_ui.view);
		m_scrollBarController->SetScrollArea(m_ui.view);

		m_ui.view->setModel(m_model.get());
		m_ui.view->setAlternatingRowColors(m_settings->Get(Constant::Settings::PREFER_ALTERNATING_ROW_COLORS, false));

		auto& header = *m_ui.view->header();
		header.setDefaultAlignment(Qt::AlignCenter);
		header.setSectionResizeMode(Column::Title, QHeaderView::Stretch);
		header.setSectionResizeMode(Column::Hotkey, QHeaderView::ResizeToContents);
		header.setSectionResizeMode(Column::Icon, QHeaderView::Fixed);
		header.setSectionResizeMode(Column::Hidden, QHeaderView::Fixed);

		m_ui.view->setItemDelegateForColumn(Column::Hotkey, new HotkeyDelegate(&m_self));
		//		m_ui.view->setItemDelegateForColumn(Column::Icon, new IconCenterDelegate(&m_self));

		m_ui.view->setCurrentIndex({});
		for (int i = 0, sz = m_model->rowCount(); i < sz; ++i)
			m_ui.view->expand(m_model->index(i, 0));

		connect(m_ui.view, &QWidget::customContextMenuRequested, &self, [this] {
			CreateContextMenu();
		});

		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
	}

	void OnShowEvent(QShowEvent*)
	{
		const auto cellHeight = m_ui.view->visualRect(m_model->index(0, 0)).height();
		auto&      header     = *m_ui.view->header();
		for (int i = 0, sz = header.count(); i < sz; ++i)
			if (header.sectionResizeMode(i) == QHeaderView::Fixed)
				header.resizeSection(i, cellHeight);

		m_hiddenNavigation = GetHiddenNavigation();
	}

	void OnHideEvent(QHideEvent*) const
	{
		m_self.done(GetHiddenNavigation() != m_hiddenNavigation ? IMenuCustomizer::Result::NeedReboot : IMenuCustomizer::Result::Ok);
	}

private:
	std::unordered_set<NavigationMode> GetHiddenNavigation() const
	{
		return NAVIGATION_NAMES | std::views::filter([this](const auto& item) {
				   return m_menuCustomizer->IsHidden(QString(Constant::Settings::NAVIGATION_HIDDEN_KEY_TEMPLATE).arg(item.first)).toBool();
			   })
		     | std::views::transform([](const auto& item) {
				   return item.second;
			   })
		     | std::ranges::to<std::unordered_set>();
	}

	void CreateContextMenu()
	{
		QMenu menu;
		menu.setFont(m_self.font());

		const auto key       = m_ui.view->currentIndex().data(Model::ModelRole::Key).toString();
		const auto abilities = m_menuCustomizer->GetAbilities(key);

		if (!!(abilities & IMenuCustomizer::ItemAbility::Hotkey))
		{
			menu.addAction(Tr(SET_HOTKEY), [this] {
				auto index = m_ui.view->currentIndex();
				index      = m_model->index(index.row(), Column::Hotkey, index.parent());
				m_ui.view->edit(index);
			});

			if (!m_menuCustomizer->GetHotkey(key).isEmpty())
				menu.addAction(Tr(REMOVE_HOTKEY), [this] {
					m_model->setData(m_ui.view->currentIndex(), {});
				});
		}

		if (m_ui.view->currentIndex().parent().isValid())
		{
			if (!menu.actions().isEmpty())
				menu.addSeparator();

			if (!!(abilities & IMenuCustomizer::ItemAbility::Icon))
				menu.addAction(Tr(SET_ICON), [&] {
					if (const auto path = m_uiFactory->GetOpenFileName(ICONS, Tr(SELECT_ICON), Tr(SELECT_ICON_FILTER)); !path.isEmpty())
					{
						if (const auto result = m_menuCustomizer->SetIcon(key, path); !result)
							m_uiFactory->ShowError(result.error());
						else
							m_model->setData(m_ui.view->currentIndex(), {}, Model::ModelRole::Icon);
					}
				});

			if (m_menuCustomizer->GetIcon(key).canConvert<QIcon>())
				menu.addAction(Tr(REMOVE_ICON), [&] {
					(void)m_menuCustomizer->SetIcon(key);
					m_model->setData(m_ui.view->currentIndex(), {}, Model::ModelRole::Icon);
				});
		}

		if (!menu.actions().isEmpty() && !menu.actions().back()->isSeparator())
			menu.addSeparator();

		Util::FillTreeContextMenu(*m_ui.view, menu).exec(QCursor::pos());
	}

private:
	QDialog& m_self;

	std::shared_ptr<const Util::IUiFactory>                       m_uiFactory;
	PropagateConstPtr<ISettings, std::shared_ptr>                 m_settings;
	PropagateConstPtr<IMenuCustomizer, std::shared_ptr>           m_menuCustomizer;
	PropagateConstPtr<QAbstractItemModel>                         m_model;
	PropagateConstPtr<Util::ItemViewToolTipper, std::shared_ptr>  m_itemViewToolTipper;
	PropagateConstPtr<Util::ScrollBarController, std::shared_ptr> m_scrollBarController;

	std::unordered_set<NavigationMode> m_hiddenNavigation;

	Ui::CustomizeMenuDialog m_ui;
};

CustomizeMenuDialog::CustomizeMenuDialog(
	const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
	const std::shared_ptr<IModelProvider>&        modelProvider,
	std::shared_ptr<const Util::IUiFactory>       uiFactory,
	std::shared_ptr<ISettings>                    settings,
	std::shared_ptr<IMenuCustomizer>              menuCustomizer,
	std::shared_ptr<Util::ItemViewToolTipper>     itemViewToolTipper,
	std::shared_ptr<Util::ScrollBarController>    scrollBarController,
	QWidget*                                      parent
)
	: QDialog(parentWidgetProvider->GetWidget(parent))
	, m_impl(*this, *modelProvider, std::move(uiFactory), std::move(settings), std::move(menuCustomizer), std::move(itemViewToolTipper), std::move(scrollBarController))
{
}

CustomizeMenuDialog::~CustomizeMenuDialog() = default;

void CustomizeMenuDialog::showEvent(QShowEvent* event)
{
	m_impl->OnShowEvent(event);
}

void CustomizeMenuDialog::hideEvent(QHideEvent* event)
{
	m_impl->OnHideEvent(event);
}
