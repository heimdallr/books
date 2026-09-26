#include "ui_SettingsDialog.h"

#include "SettingsDialog.h"

#include <expected>
#include <ranges>

#include <QIdentityProxyModel>
#include <QMenu>

#include "interface/constants/ModelRole.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"

#include "gutil/util.h"
#include "logic/data/DataItem.h"
#include "utilgui/GeometryRestorable.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace {

constexpr auto CONTEXT = "SettingsDialog";
constexpr auto KEY     = QT_TRANSLATE_NOOP("SettingsDialog", "Key");
constexpr auto VALUE   = QT_TRANSLATE_NOOP("SettingsDialog", "Value");
constexpr auto REMOVE  = QT_TRANSLATE_NOOP("SettingsDialog", "Remove");

constexpr auto FIELD_WIDTH_KEY = "ui/View/SettingsDialog/columnWidths";

TR_DEF

QString GetName(const QString& parent, const QString& key)
{
	return QString("%1%2").arg(parent, parent.isEmpty() ? key : QString("/%1").arg(key));
}

IDataItem::Ptr CreateModelData(const ISettings& settings, const IDataItemFactory& dataItemFactory)
{
	auto root = dataItemFactory.CreateSettingsItem();
	root->SetData(Tr(KEY), SettingsItem::Column::Key);
	root->SetData(Tr(VALUE), SettingsItem::Column::Value);

	const auto enumerate = [&](IDataItem& parent, const auto& r) -> void {
		for (const auto& group : settings.GetGroups())
		{
			auto child = dataItemFactory.CreateSettingsItem();
			child->SetId(GetName(parent.GetId(), group));
			child->SetData(group, SettingsItem::Column::Key);
			SettingsGroup settingsGroup(settings, group);
			r(*child, r);
			parent.AppendChild(std::move(child));
		}

		for (const auto& key : settings.GetKeys())
		{
			auto child = dataItemFactory.CreateSettingsItem();
			child->SetId(GetName(parent.GetId(), key));
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

class Model final : public QIdentityProxyModel
{
public:
	static std::unique_ptr<QAbstractItemModel> Create(const IModelProvider& modelProvider, std::shared_ptr<ISettings> settings, const IDataItemFactory& dataItemFactory)
	{
		auto model = modelProvider.CreateTreeModel(CreateModelData(*settings, dataItemFactory));
		model->setData({}, 1, Role::CheckableColumn);
		return std::make_unique<Model>(std::move(model), std::move(settings));
	}

	Model(std::shared_ptr<QAbstractItemModel> source, std::shared_ptr<ISettings> settings, QObject* parent = nullptr)
		: QIdentityProxyModel(parent)
		, m_source { std::move(source) }
		, m_settings { std::move(settings) }
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
		if (index.column() != SettingsItem::Column::Value)
			return QIdentityProxyModel::data(index, role);

		switch (role)
		{
			case Qt::DisplayRole:
				if (const auto checked = GetChecked(index); !checked.has_value())
					return checked.error();
				return {};

			case Qt::CheckStateRole:
				if (const auto checked = GetChecked(index))
					return *checked;
				return {};

			default:
				break;
		}

		return QIdentityProxyModel::data(index, role);
	}

	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		if (index.column() == SettingsItem::Column::Value)
		{
			switch (role)
			{
				case Qt::EditRole:
					m_settings->Set(index.data(Role::Id).toString(), value);
					QIdentityProxyModel::setData(index, value, Role::FirstItemColumn + SettingsItem::Column::Value);
					emit dataChanged(index, index, { Qt::DisplayRole });
					return true;

				case Qt::CheckStateRole:
				{
					const auto checked = value.value<Qt::CheckState>() == Qt::Checked;
					m_settings->Set(index.data(Role::Id).toString(), checked);
					QIdentityProxyModel::setData(index, QString(checked ? "true" : "false"), Role::FirstItemColumn + SettingsItem::Column::Value);
					emit dataChanged(index, index, { Qt::CheckStateRole });
					return true;
				}

				default:
					break;
			}
		}

		assert(false && "unexpected column or role");
		return false;
	}

	Qt::ItemFlags flags(const QModelIndex& index) const override
	{
		auto result = QIdentityProxyModel::flags(index);
		if (index.column() == 0)
			return result;

		result |= Qt::ItemIsEnabled | Qt::ItemIsSelectable;
		result |= GetChecked(index) ? Qt::ItemIsUserCheckable : Qt::ItemIsEditable;

		return result;
	}

private:
	std::expected<Qt::CheckState, QString> GetChecked(const QModelIndex& index) const
	{
		const auto value = mapToSource(index).data(Qt::DisplayRole).toString();
		return value == "true" ? std::expected<Qt::CheckState, QString> { Qt::Checked } : value == "false" ? std::expected<Qt::CheckState, QString> { Qt::Unchecked } : std::unexpected(value);
	}

private:
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_source;
	PropagateConstPtr<ISettings, std::shared_ptr>          m_settings;
};

} // namespace

class SettingsDialog::Impl final
    : Util::GeometryRestorable
    , Util::GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		QDialog&                                   self,
		const IModelProvider&                      modelProvider,
		const IDataItemFactory&                    dataItemFactory,
		std::shared_ptr<ISettings>                 settings,
		std::shared_ptr<Util::ItemViewToolTipper>  itemViewToolTipper,
		std::shared_ptr<Util::ScrollBarController> scrollBarController
	)
		: GeometryRestorable(*this, settings, CONTEXT)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_settings { std::move(settings) }
		, m_model { Model::Create(modelProvider, m_settings, dataItemFactory) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
	{
		m_ui.setupUi(&self);

		m_itemViewToolTipper->SetScrollArea(m_ui.view);
		m_scrollBarController->SetScrollArea(m_ui.view);

		m_ui.view->setModel(m_model.get());
		m_ui.view->header()->setDefaultAlignment(Qt::AlignCenter);
		m_ui.view->setAlternatingRowColors(m_settings->Get(Constant::Settings::PREFER_ALTERNATING_ROW_COLORS, false));

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
	QWidget&                                                      m_self;
	Ui::SettingsDialog                                            m_ui;
	PropagateConstPtr<ISettings, std::shared_ptr>                 m_settings;
	PropagateConstPtr<QAbstractItemModel>                         m_model;
	PropagateConstPtr<Util::ItemViewToolTipper, std::shared_ptr>  m_itemViewToolTipper;
	PropagateConstPtr<Util::ScrollBarController, std::shared_ptr> m_scrollBarController;

	QStringList m_keysToRemove;
};

SettingsDialog::SettingsDialog(
	const std::shared_ptr<const IParentWidgetProvider>& parentWidgetProvider,
	const std::shared_ptr<const IModelProvider>&        modelProvider,
	const std::shared_ptr<const IDataItemFactory>&      dataItemFactory,
	std::shared_ptr<ISettings>                          settings,
	std::shared_ptr<Util::ItemViewToolTipper>           itemViewToolTipper,
	std::shared_ptr<Util::ScrollBarController>          scrollBarController,
	QWidget*                                            parent
)
	: QDialog(parentWidgetProvider->GetWidget(parent))
	, m_impl(*this, *modelProvider, *dataItemFactory, std::move(settings), std::move(itemViewToolTipper), std::move(scrollBarController))
{
}

SettingsDialog::~SettingsDialog() = default;
