#include "ui_TreeView.h"
#include "TreeView.h"

#include <ranges>

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
		m_controller->UnregisterObserver(this);
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
		RestoreValue();
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

		m_filterTimer.setSingleShot(true);
		m_filterTimer.setInterval(std::chrono::milliseconds(200));

		FillComboBoxes();
		Connect();

		OnModeChanged(m_controller->GetModeIndex());
		m_controller->RegisterObserver(this);
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
	}

	void Connect()
	{
		connect(m_ui.cbMode, &QComboBox::currentIndexChanged, &m_self, [&] (const int index)
		{
			m_controller->SetModeIndex(index);
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

	void SaveValue()
	{
		m_settings->Set(GetValueModeValueKey(), m_ui.value->text());
	}

	void RestoreValue()
	{
		m_ui.value->setText(m_settings->Get(GetValueModeValueKey()).toString());
		m_settings->Set(GetValueModeKey(), m_ui.cbValueMode->currentData().toString());
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
