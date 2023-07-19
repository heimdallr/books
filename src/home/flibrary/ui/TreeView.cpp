#include "ui_TreeView.h"
#include "TreeView.h"

#include <QTimer>
#include <plog/Log.h>

#include "interface/constants/Enums.h"
#include "interface/logic/ITreeViewController.h"

#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto VALUE_MODE_KEY = "ui/%1/%2/%3/Value";

constexpr const char * VALUE_MODE[]
{
	QT_TRANSLATE_NOOP("TreeView", "Find"),
	QT_TRANSLATE_NOOP("TreeView", "Filter"),
};

}

struct TreeView::Impl final
	: private ITreeViewController::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Ui::TreeView ui{};
	TreeView & self;
	PropagateConstPtr<ITreeViewController, std::shared_ptr> controller;
	PropagateConstPtr<ISettings, std::shared_ptr> settings;

	Impl(TreeView & self
		, std::shared_ptr<ITreeViewController> controller
		, std::shared_ptr<ISettings> settings
	)
		: self(self)
		, controller(std::move(controller))
		, settings(std::move(settings))
	{
		ui.setupUi(&this->self);
		ui.treeView->setHeaderHidden(this->controller->GetItemType() == ItemType::Navigation);

		for (const auto * name : VALUE_MODE)
			ui.cbValueMode->addItem(QIcon(QString(":/icons/%1.png").arg(name)), "", QString(name));

		ui.cbValueMode->setStyleSheet("QComboBox::drop-down {border-width: 0px;} QComboBox::down-arrow {image: url(noimg); border-width: 0px;}");
		for (const auto * name : this->controller->GetModeNames())
			ui.cbMode->addItem(QCoreApplication::translate(this->controller->TrContext(), name), QString(name));

		connect(ui.cbMode, &QComboBox::currentIndexChanged, [this] (const int index) { this->controller->SetModeIndex(index); });
		connect(ui.cbValueMode, &QComboBox::currentIndexChanged, [this] { RestoreValue(); });
		connect(ui.value, &QLineEdit::textEdited, [&] { SaveValue(); });
		connect(ui.value, &QLineEdit::textChanged, [&] { OnValueChanged(); });

		this->controller->RegisterObserver(this);
		OnModeChanged(this->controller->GetModeIndex());
	}

	~Impl() override
	{
		controller->UnregisterObserver(this);
	}

private: // ITreeViewController::IObserver
	void OnModeChanged(const int index) override
	{
		ui.cbMode->setCurrentIndex(index);
	}

	void OnModelChanged(QAbstractItemModel * model) override
	{
		ui.treeView->setModel(model);
		ui.treeView->setRootIsDecorated(controller->GetViewMode() == ViewMode::Tree);
		RestoreValue();
	}

private:
	auto GetValueModeKey() const
	{
		return QString(VALUE_MODE_KEY).arg(this->controller->TrContext()).arg(ui.cbMode->currentData().toString()).arg(ui.cbValueMode->currentData().toString());
	}

	void SaveValue()
	{
		settings->Set(GetValueModeKey(), ui.value->text());
	}

	void RestoreValue()
	{
		ui.value->setText(settings->Get(GetValueModeKey()).toString());
	}

	void OnValueChanged() const
	{
		switch (ui.cbValueMode->currentIndex())
		{
			case 0:
				if (const auto matched = ui.treeView->model()->match(ui.treeView->model()->index(0, 0), Qt::DisplayRole, ui.value->text(), 1, Qt::MatchFlag::MatchStartsWith | Qt::MatchFlag::MatchRecursive); !matched.isEmpty())
					ui.treeView->setCurrentIndex(matched.front());
				break;

			default:
				break;
		}
	}
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
