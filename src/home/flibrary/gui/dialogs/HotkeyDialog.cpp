#include "ui_HotkeyDialog.h"

#include "HotkeyDialog.h"

#include <QIdentityProxyModel>
#include <QMenu>

#include "gutil/util.h"
#include "logic/data/DataItem.h"
#include "util/GeometryRestorable.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto CONTEXT         = "HotkeyDialog";
constexpr auto FIELD_WIDTH_KEY = "ui/View/HotkeyDialog/columnWidths";

class Model final : public QIdentityProxyModel
{
public:
	static std::unique_ptr<QAbstractItemModel> Create(const IModelProvider& modelProvider, const IHotkeyManager& hotkeyManager)
	{
		return std::make_unique<Model>(modelProvider.CreateTreeModel(hotkeyManager.GetRootDataItem()));
	}

	explicit Model(std::shared_ptr<QAbstractItemModel> source, QObject* parent = nullptr)
		: QIdentityProxyModel(parent)
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

private:
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_source;
};

}

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
	void CreateContextMenu() const
	{
		QMenu menu;
		menu.setFont(m_self.font());
		Util::FillTreeContextMenu(*m_ui.view, menu).exec(QCursor::pos());
	}

private:
	QDialog& m_self;

	PropagateConstPtr<ISettings, std::shared_ptr>           m_settings;
	PropagateConstPtr<IHotkeyManager, std::shared_ptr>      m_hotkeyManager;
	PropagateConstPtr<QAbstractItemModel>  m_model;
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
