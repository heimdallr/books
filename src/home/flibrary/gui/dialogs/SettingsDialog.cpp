#include "ui_SettingsDialog.h"

#include "SettingsDialog.h"

#include <ranges>

#include <QMenu>

#include "interface/Localization.h"

#include "gutil/GeometryRestorable.h"
#include "gutil/util.h"
#include "logic/data/DataItem.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto CONTEXT = "SettingsDialog";
constexpr auto KEY     = QT_TRANSLATE_NOOP("SettingsDialog", "Key");
constexpr auto VALUE   = QT_TRANSLATE_NOOP("SettingsDialog", "Value");
constexpr auto REMOVE  = QT_TRANSLATE_NOOP("SettingsDialog", "Remove");

constexpr auto FIELD_WIDTH_KEY = "ui/View/SettingsDialog/columnWidths";

TR_DEF

IDataItem::Ptr CreateModelData(const ISettings& settings)
{
	auto root = SettingsItem::Create();
	root->SetData(Tr(KEY), SettingsItem::Column::Key);
	root->SetData(Tr(VALUE), SettingsItem::Column::Value);

	const auto enumerate = [&](IDataItem& parent, const auto& r) -> void {
		for (const auto& group : settings.GetGroups())
		{
			auto child = SettingsItem::Create();
			child->SetData(group, SettingsItem::Column::Key);
			SettingsGroup settingsGroup(settings, group);
			r(*child, r);
			parent.AppendChild(std::move(child));
		}

		for (const auto& key : settings.GetKeys())
		{
			auto child = SettingsItem::Create();
			child->SetData(key, SettingsItem::Column::Key);
			child->SetData(settings.Get(key).toString(), SettingsItem::Column::Value);
			parent.AppendChild(std::move(child));
		}
	};

	enumerate(*root, enumerate);

	return root;
}

QString GetKey(QModelIndex index)
{
	QStringList result;
	for (; index.isValid(); index = index.parent())
		result.push_front(index.data().toString());
	return result.join('/');
}

} // namespace

class SettingsDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		QDialog&                             self,
		const IModelProvider&                modelProvider,
		std::shared_ptr<ISettings>           settings,
		std::shared_ptr<ItemViewToolTipper>  itemViewToolTipper,
		std::shared_ptr<ScrollBarController> scrollBarController
	)
		: GeometryRestorable(*this, settings, CONTEXT)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_settings { std::move(settings) }
		, m_model { modelProvider.CreateTreeModel(CreateModelData(*m_settings)) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
	{
		m_ui.setupUi(&self);

		m_scrollBarController->SetScrollArea(m_ui.view);

		m_ui.view->setModel(m_model.get());
		m_ui.view->viewport()->installEventFilter(m_itemViewToolTipper.get());
		m_ui.view->viewport()->installEventFilter(m_scrollBarController.get());

		m_ui.view->header()->setDefaultAlignment(Qt::AlignCenter);

		connect(&self, &QDialog::accepted, &self, [this] {
			RemoveImpl();
		});

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
		connect(menu.addAction(Tr(REMOVE)), &QAction::triggered, &m_self, [this] {
			const auto indices = m_ui.view->selectionModel()->selectedIndexes() | std::views::filter([](const auto& item) {
									 return item.column() == 0;
								 })
			                   | std::ranges::to<std::vector<QPersistentModelIndex>>();
			for (const auto& index : indices)
			{
				if (!index.isValid())
					continue;

				m_keysToRemove << GetKey(index);
				m_model->removeRow(index.row(), index.parent());
			}
		});
		menu.setFont(m_self.font());
		Util::FillTreeContextMenu(*m_ui.view, menu).exec(QCursor::pos());
	}

	void RemoveImpl()
	{
		for (const auto& key : m_keysToRemove)
			m_settings->Remove(key);
	}

private:
	QWidget&                                                m_self;
	Ui::SettingsDialog                                      m_ui;
	PropagateConstPtr<ISettings, std::shared_ptr>           m_settings;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr>  m_model;
	PropagateConstPtr<ItemViewToolTipper, std::shared_ptr>  m_itemViewToolTipper;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> m_scrollBarController;

	QStringList m_keysToRemove;
};

SettingsDialog::SettingsDialog(
	const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
	const std::shared_ptr<IModelProvider>&        modelProvider,
	std::shared_ptr<ISettings>                    settings,
	std::shared_ptr<ItemViewToolTipper>           itemViewToolTipper,
	std::shared_ptr<ScrollBarController>          scrollBarController,
	QWidget*                                      parent
)
	: QDialog(parentWidgetProvider->GetWidget(parent))
	, m_impl(*this, *modelProvider, std::move(settings), std::move(itemViewToolTipper), std::move(scrollBarController))
{
}

SettingsDialog::~SettingsDialog() = default;

int SettingsDialog::Exec()
{
	return exec();
}
