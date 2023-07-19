#include "ui_TreeView.h"
#include "TreeView.h"

#include <QTimer>
#include <ranges>
#include <plog/Log.h>

#include "interface/constants/Enums.h"
#include "interface/logic/ITreeViewController.h"

#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto VALUE_MODE_ICON_TEMPLATE = ":/icons/%1.png";
constexpr auto VALUE_MODE_KEY = "ui/%1/%2/%3/Value";
constexpr auto VALUE_MODE_STYLE_SHEET = "QComboBox::drop-down {border-width: 0px;} QComboBox::down-arrow {image: url(noimg); border-width: 0px;}";

class IValueApplier  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IValueApplier() = default;
	virtual void Find() const = 0;
	virtual void Filter() const = 0;
};

using ApplyValue = void(IValueApplier::*)() const;

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
		RestoreValue();
	}

private: //	IValueApplier
	void Find() const override
	{
		if (const auto matched = m_ui.treeView->model()->match(m_ui.treeView->model()->index(0, 0), Qt::DisplayRole, m_ui.value->text(), 1, Qt::MatchFlag::MatchStartsWith | Qt::MatchFlag::MatchRecursive); !matched.isEmpty())
			m_ui.treeView->setCurrentIndex(matched.front());
	}

	void Filter() const override
	{
	}

private:
	void Setup()
	{
		m_ui.setupUi(&m_self);
		m_ui.treeView->setHeaderHidden(m_controller->GetItemType() == ItemType::Navigation);

		FillComboBoxes();
		Connect();

		OnModeChanged(m_controller->GetModeIndex());
		m_controller->RegisterObserver(this);
	}

	void FillComboBoxes()
	{
		for (const auto * name : VALUE_MODES | std::views::keys)
			m_ui.cbValueMode->addItem(QIcon(QString(VALUE_MODE_ICON_TEMPLATE).arg(name)), "", QString(name));

		m_ui.cbValueMode->setStyleSheet(VALUE_MODE_STYLE_SHEET);
		for (const auto * name : m_controller->GetModeNames())
			m_ui.cbMode->addItem(QCoreApplication::translate(m_controller->TrContext(), name), QString(name));
	}

	void Connect()
	{
		connect(m_ui.cbMode     , &QComboBox::currentIndexChanged, [&] (const int index) { m_controller->SetModeIndex(index); });
		connect(m_ui.cbValueMode, &QComboBox::currentIndexChanged, [&]                   { RestoreValue(); });
		connect(m_ui.value      , &QLineEdit::textEdited         , [&]                   { SaveValue(); });
		connect(m_ui.value      , &QLineEdit::textChanged        , [&]                   { OnValueChanged(); });
	}

	auto GetValueModeKey() const
	{
		return QString(VALUE_MODE_KEY).arg(this->m_controller->TrContext()).arg(m_ui.cbMode->currentData().toString()).arg(m_ui.cbValueMode->currentData().toString());
	}

	void SaveValue()
	{
		m_settings->Set(GetValueModeKey(), m_ui.value->text());
	}

	void RestoreValue()
	{
		m_ui.value->setText(m_settings->Get(GetValueModeKey()).toString());
	}

	void OnValueChanged() const
	{
		((*this).*VALUE_MODES[m_ui.cbValueMode->currentIndex()].second)();
	}

private:
	TreeView & m_self;
	PropagateConstPtr<ITreeViewController, std::shared_ptr> m_controller;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	Ui::TreeView m_ui {};
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
