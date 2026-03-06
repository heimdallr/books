#include "ui_HotkeyDialog.h"

#include "HotkeyDialog.h"

#include <QIdentityProxyModel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QStyledItemDelegate>

#include "fnd/IsOneOf.h"

#include "interface/localization.h"

#include "gutil/util.h"
#include "logic/data/DataItem.h"
#include "util/GeometryRestorable.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto CONTEXT = "HotkeyDialog";
constexpr auto RESET   = QT_TRANSLATE_NOOP("HotkeyDialog", "Reset");

TR_DEF

constexpr auto FIELD_WIDTH_KEY = "ui/View/HotkeyDialog/columnWidths";

class Model final : public QIdentityProxyModel
{
public:
	static std::unique_ptr<QAbstractItemModel> Create(const IModelProvider& modelProvider, IHotkeyManager& hotkeyManager)
	{
		return std::make_unique<Model>(hotkeyManager, modelProvider.CreateTreeModel(hotkeyManager.GetRootDataItem()));
	}

	Model(IHotkeyManager& hotkeyManager, std::shared_ptr<QAbstractItemModel> source, QObject* parent = nullptr)
		: QIdentityProxyModel(parent)
		, m_hotkeyManager { hotkeyManager }
		, m_source { std::move(source) }
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
		if (!index.isValid() || role != Qt::DisplayRole || index.column() != 0)
			return QIdentityProxyModel::data(index, role);

		const auto sourceIndex = mapToSource(index);
		return m_source->index(sourceIndex.row(), SettingsItem::Column::Title, sourceIndex.parent()).data(role);
	}

	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		if (!index.isValid() || role != Qt::EditRole)
			return false;

		const auto shortCut = value.toString();
		if (shortCut.isEmpty())
		{
			m_hotkeyManager.Reset(GetKey(index));
			for (int row = 0, sz = rowCount(index); row < sz; ++row)
				m_hotkeyManager.Reset(GetKey(this->index(row, 0, index)));
			return true;
		}

		m_hotkeyManager.Set(GetKey(index), shortCut);
		emit dataChanged(index, index);
		return true;
	}

	Qt::ItemFlags flags(const QModelIndex& index) const override
	{
		return QIdentityProxyModel::flags(index) | (index.column() == 1 && !!m_hotkeyManager.GetAction(GetKey(index)) ? Qt::ItemIsEditable : Qt::NoItemFlags);
	}

private:
	QString GetKey(const QModelIndex& index) const
	{
		const auto sourceIndex = mapToSource(index);
		return m_source->index(sourceIndex.row(), SettingsItem::Column::Key, sourceIndex.parent()).data().toString();
	}

private:
	IHotkeyManager& m_hotkeyManager;

	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_source;
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
		auto* lineEdit = qobject_cast<QLineEdit*>(editor);
		model->setData(index, lineEdit->text());
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
		QDialog&                             self,
		const IModelProvider&                modelProvider,
		std::shared_ptr<ISettings>           settings,
		std::shared_ptr<IHotkeyManager>      hotkeyManager,
		std::shared_ptr<ItemViewToolTipper>  itemViewToolTipper,
		std::shared_ptr<ScrollBarController> scrollBarController
	)
		: GeometryRestorable(*this, settings, CONTEXT)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_settings { std::move(settings) }
		, m_hotkeyManager { std::move(hotkeyManager) }
		, m_model { Model::Create(modelProvider, *m_hotkeyManager) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
	{
		m_ui.setupUi(&m_self);
		m_ui.view->setModel(m_model.get());
		m_ui.view->viewport()->installEventFilter(m_itemViewToolTipper.get());
		m_ui.view->viewport()->installEventFilter(m_scrollBarController.get());
		m_ui.view->header()->setDefaultAlignment(Qt::AlignCenter);
		m_ui.view->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
		m_ui.view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
		m_ui.view->setItemDelegateForColumn(1, new HotkeyDelegate(&m_self));
		m_ui.view->expand(m_model->index(0, 0));
		m_ui.view->setCurrentIndex({});

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
		menu.addAction(Tr(RESET), [this] {
			m_model->setData(m_ui.view->currentIndex(), {});
		});
		Util::FillTreeContextMenu(*m_ui.view, menu).exec(QCursor::pos());
	}

private:
	QDialog& m_self;

	PropagateConstPtr<ISettings, std::shared_ptr>           m_settings;
	PropagateConstPtr<IHotkeyManager, std::shared_ptr>      m_hotkeyManager;
	PropagateConstPtr<QAbstractItemModel>                   m_model;
	PropagateConstPtr<ItemViewToolTipper, std::shared_ptr>  m_itemViewToolTipper;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> m_scrollBarController;

	Ui::HotkeyDialog m_ui;
};

HotkeyDialog::HotkeyDialog(
	const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
	const std::shared_ptr<IModelProvider>&        modelProvider,
	std::shared_ptr<ISettings>                    settings,
	std::shared_ptr<IHotkeyManager>               hotkeyManager,
	std::shared_ptr<ItemViewToolTipper>           itemViewToolTipper,
	std::shared_ptr<ScrollBarController>          scrollBarController,
	QWidget*                                      parent
)
	: QDialog(parentWidgetProvider->GetWidget(parent))
	, m_impl(*this, *modelProvider, std::move(settings), std::move(hotkeyManager), std::move(itemViewToolTipper), std::move(scrollBarController))
{
}

HotkeyDialog::~HotkeyDialog() = default;
