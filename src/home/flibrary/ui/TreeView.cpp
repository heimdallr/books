#include "ui_TreeView.h"
#include "TreeView.h"

#include <qevent.h>
#include <ranges>

#include <QMenu>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTimer>

#include <plog/Log.h>

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"
#include "interface/logic/ITreeViewController.h"

#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto VALUE_MODE_ICON_TEMPLATE = ":/icons/%1.png";
constexpr auto VALUE_MODE_VALUE_KEY = "ui/%1/%2/%3/Value";
constexpr auto VALUE_MODE_KEY = "ui/%1/ValueMode";
constexpr auto VALUE_MODE_STYLE_SHEET = "QComboBox::drop-down {border-width: 0px;} QComboBox::down-arrow {image: url(noimg); border-width: 0px;}";
constexpr auto COLUMNS_KEY = "ui/%1/%2/Columns/%3";
constexpr auto COLUMN_WIDTH_LOCAL_KEY = "%1/Width";
constexpr auto COLUMN_INDEX_LOCAL_KEY = "%1/Index";
constexpr auto COLUMN_HIDDEN_LOCAL_KEY = "%1/Hidden";

class IValueApplier  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IValueApplier() = default;
	virtual void Find() = 0;
	virtual void Filter() = 0;
};

using ApplyValue = void(IValueApplier::*)();

constexpr std::pair<const char *, ApplyValue> VALUE_MODES[]
{
	{ QT_TRANSLATE_NOOP("TreeView", "Find"), &IValueApplier::Find },
	{ QT_TRANSLATE_NOOP("TreeView", "Filter"), &IValueApplier::Filter },
};

class Delegate final : public QStyledItemDelegate
{
public:
	explicit Delegate(QAbstractScrollArea * parent = nullptr)
		: QStyledItemDelegate(parent)
		, m_view(*parent->viewport())
	{
	}

private:
	void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override
	{
		if (index.data(Role::Type).value<ItemType>() == ItemType::Books)
			return QStyledItemDelegate::paint(painter, option, index);

		if (index.column() != 0)
			return;

		auto o = option;
		o.rect.setWidth(m_view.width() - QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent) - o.rect.x());
		QStyledItemDelegate::paint(painter, o, index);
	}

private:
	QWidget & m_view;
};

}

class TreeView::Impl final
	: ITreeViewController::IObserver
	, IValueApplier
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(TreeView & self
		, std::shared_ptr<ITreeViewController> controller
		, std::shared_ptr<ISettings> settings
	)
		: m_self(self)
		, m_controller(std::move(controller))
		, m_settings(std::move(settings))
	{
		Setup();
	}

	~Impl() override
	{
		SaveHeaderLayout();
		m_controller->UnregisterObserver(this);
	}

	void SetNavigationModeName(QString navigationModeName)
	{
		SaveHeaderLayout();
		m_navigationModeName = std::move(navigationModeName);
	}

	void ResizeEvent(const QResizeEvent * event)
	{
		if (m_controller->GetItemType() != ItemType::Books || m_recentMode.isEmpty() || m_navigationModeName.isEmpty())
			return;

		m_ui.treeView->header()->resizeSection(0, m_ui.treeView->header()->sectionSize(0) + (event->size().width() - event->oldSize().width()));
	}

private: // ITreeViewController::IObserver
	void OnModeChanged(const int index) override
	{
		m_ui.cbMode->setCurrentIndex(index);
	}

	void OnModelChanged(QAbstractItemModel * model) override
	{
		m_ui.treeView->setModel(model);
		m_ui.treeView->setRootIsDecorated(m_controller->GetViewMode() == ViewMode::Tree);
		connect(m_ui.treeView->selectionModel(), &QItemSelectionModel::currentRowChanged, &m_self, [&] (const QModelIndex & index)
		{
			m_controller->SetCurrentId(index.data(Role::Id).toString());
		});

		if (m_controller->GetItemType() == ItemType::Books)
		{
			auto * widget = m_ui.treeView->header();
			widget->setStretchLastSection(false);
			widget->setContextMenuPolicy(Qt::CustomContextMenu);
			m_ui.treeView->setItemDelegate(new Delegate(m_ui.treeView));
			connect(widget, &QWidget::customContextMenuRequested, &m_self, [&] (const QPoint & pos)
			{
				CreateHeaderContextMenu(pos);
			});
		}

		RestoreValue();
		RestoreHeaderLayout();
	}

private: //	IValueApplier
	void Find() override
	{
		if (const auto matched = m_ui.treeView->model()->match(m_ui.treeView->model()->index(0, 0), Qt::DisplayRole, m_ui.value->text(), 1, Qt::MatchFlag::MatchStartsWith | Qt::MatchFlag::MatchRecursive); !matched.isEmpty())
			m_ui.treeView->setCurrentIndex(matched.front());
	}

	void Filter() override
	{
		m_filterTimer.start();
	}

private:
	void Setup()
	{
		m_ui.setupUi(&m_self);
		m_ui.treeView->setHeaderHidden(m_controller->GetItemType() == ItemType::Navigation);
		//m_ui.treeView->setItemDelegate(new Delegate(&m_self));

		m_filterTimer.setSingleShot(true);
		m_filterTimer.setInterval(std::chrono::milliseconds(200));

		FillComboBoxes();
		Connect();

		OnModeChanged(m_controller->GetModeIndex());
		m_controller->RegisterObserver(this);

		QTimer::singleShot(0, [&] { emit m_self.NavigationModeNameChanged(m_ui.cbMode->currentData().toString()); });
	}

	void FillComboBoxes()
	{
		m_ui.cbValueMode->setStyleSheet(VALUE_MODE_STYLE_SHEET);
		for (const auto * name : VALUE_MODES | std::views::keys)
			m_ui.cbValueMode->addItem(QIcon(QString(VALUE_MODE_ICON_TEMPLATE).arg(name)), "", QString(name));

		for (const auto * name : m_controller->GetModeNames())
			m_ui.cbMode->addItem(QCoreApplication::translate(m_controller->TrContext(), name), QString(name));

		const auto it = std::ranges::find_if(VALUE_MODES, [mode = m_settings->Get(GetValueModeKey()).toString()] (const auto & item) { return mode == item.first; });
		if (it != std::cend(VALUE_MODES))
			m_ui.cbValueMode->setCurrentIndex(static_cast<int>(std::distance(std::cbegin(VALUE_MODES), it)));

		m_recentMode = m_ui.cbMode->currentData().toString();
	}

	void Connect()
	{
		connect(m_ui.cbMode, &QComboBox::currentIndexChanged, &m_self, [&] (const int index)
		{
			auto newMode = m_ui.cbMode->currentData().toString();
			emit m_self.NavigationModeNameChanged(newMode);
			m_controller->SetModeIndex(index);
			SaveHeaderLayout();
			m_recentMode = std::move(newMode);
		});
		connect(m_ui.cbValueMode, &QComboBox::currentIndexChanged, &m_self, [&]
		{
			RestoreValue();
		});
		connect(m_ui.value, &QLineEdit::textEdited, &m_self, [&]
		{
			SaveValue();
		});
		connect(m_ui.value, &QLineEdit::textChanged, &m_self, [&]
		{
			OnValueChanged();
		});
		connect(&m_filterTimer, &QTimer::timeout, &m_self, [&]
		{
			m_ui.treeView->model()->setData({}, m_ui.value->text(), Role::Filter);
		});
	}

	QString GetColumnSettingsKey() const
	{
		return QString(COLUMNS_KEY)
			.arg(m_controller->TrContext())
			.arg(m_recentMode)
			.arg(m_navigationModeName)
			;
	}

	void SaveValue()
	{
		m_settings->Set(GetValueModeValueKey(), m_ui.value->text());
	}

	void RestoreValue()
	{
		m_ui.value->setText(m_settings->Get(GetValueModeValueKey()).toString());
		m_settings->Set(GetValueModeKey(), m_ui.cbValueMode->currentData().toString());
	}

	void SaveHeaderLayout()
	{
		if (m_controller->GetItemType() != ItemType::Books || m_recentMode.isEmpty() || m_navigationModeName.isEmpty())
			return;

		const auto * header = m_ui.treeView->header();

		SettingsGroup guard(*m_settings, GetColumnSettingsKey());
		for (int i = 1, sz = header->count(); i < sz; ++i)
		{
			if (!header->isSectionHidden(i))
				m_settings->Set(QString(COLUMN_WIDTH_LOCAL_KEY).arg(i), header->sectionSize(i));
			m_settings->Set(QString(COLUMN_INDEX_LOCAL_KEY).arg(i), header->visualIndex(i));
			m_settings->Set(QString(COLUMN_HIDDEN_LOCAL_KEY).arg(i), header->isSectionHidden(i));
		}
	}

	void RestoreHeaderLayout()
	{
		if (m_controller->GetItemType() != ItemType::Books || m_recentMode.isEmpty() || m_navigationModeName.isEmpty())
			return;

		auto * header = m_ui.treeView->header();
		std::map<int, int> widths;
		std::map<int, int> indices;
		{
			SettingsGroup guard(*m_settings, GetColumnSettingsKey());
			for (const auto & column : m_settings->GetGroups())
			{
				bool ok = false;
				if (const auto logicalIndex = column.toInt(&ok); ok)
				{
					widths.emplace(logicalIndex, m_settings->Get(QString(COLUMN_WIDTH_LOCAL_KEY).arg(column), -1).toInt());
					indices.emplace(m_settings->Get(QString(COLUMN_INDEX_LOCAL_KEY).arg(column), -1).toInt(), logicalIndex);
					if (m_settings->Get(QString(COLUMN_HIDDEN_LOCAL_KEY).arg(column), false).toBool())
						header->hideSection(logicalIndex);
				}
			}
		}

		indices.erase(-1);

		auto totalWidth = m_ui.treeView->viewport()->width() - QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent);

		for (int i = header->count() - 1; i > 0; --i)
		{
			const auto width = [&]
			{
				if (const auto it = widths.find(i); it != widths.end())
				{
					header->resizeSection(i, it->second);
					return it->second;
				}

				return header->sectionSize(i);
			}();

			if (!header->isSectionHidden(i))
				totalWidth -= width;
		}

		header->resizeSection(0, totalWidth);

		for (const auto [visualIndex, logicalIndex] : indices)
			header->moveSection(header->visualIndex(logicalIndex), visualIndex);
	}

	void CreateHeaderContextMenu(const QPoint & pos)
	{
		m_headerContextMenu.clear();
		auto * header = m_ui.treeView->header();
		const auto * model = header->model();
		for (int i = 0, sz = header->count(); i < sz; ++i)
		{
			const auto index = header->logicalIndex(i);
			auto * action = m_headerContextMenu.addAction(model->headerData(index, Qt::Horizontal).toString(), &m_self, [=](const bool checked)
			{
				if (!checked)
					header->resizeSection(0, header->sectionSize(0) + header->sectionSize(index));
				header->setSectionHidden(index, !checked);
				if (checked)
					header->resizeSection(0, header->sectionSize(0) - header->sectionSize(index));
			});
			action->setCheckable(true);
			action->setChecked(!header->isSectionHidden(index));
		}
		m_headerContextMenu.popup(header->mapToGlobal(pos));
	}

	void OnValueChanged()
	{
		((*this).*VALUE_MODES[m_ui.cbValueMode->currentIndex()].second)();
	}

	QString GetValueModeValueKey() const
	{
		return QString(VALUE_MODE_VALUE_KEY).arg(m_controller->TrContext()).arg(m_ui.cbMode->currentData().toString()).arg(m_ui.cbValueMode->currentData().toString());
	}

	QString GetValueModeKey() const
	{
		return QString(VALUE_MODE_KEY).arg(m_controller->TrContext());
	}

private:
	TreeView & m_self;
	PropagateConstPtr<ITreeViewController, std::shared_ptr> m_controller;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	Ui::TreeView m_ui {};
	QTimer m_filterTimer;
	QString m_navigationModeName;
	QString m_recentMode;
	QMenu m_headerContextMenu;
};

TreeView::TreeView(std::shared_ptr<ITreeViewController> controller
	, std::shared_ptr<ISettings> settings
	, QWidget * parent
)
	: QWidget(parent)
	, m_impl(*this
		, std::move(controller)
		, std::move(settings)
	)
{
	PLOGD << "TreeView created";
}

TreeView::~TreeView()
{
	PLOGD << "TreeView destroyed";
}

void TreeView::SetNavigationModeName(QString navigationModeName)
{
	m_impl->SetNavigationModeName(std::move(navigationModeName));
}

void TreeView::resizeEvent(QResizeEvent * event)
{
	m_impl->ResizeEvent(event);
	QWidget::resizeEvent(event);
}
