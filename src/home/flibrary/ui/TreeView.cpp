#include "ui_TreeView.h"
#include "TreeView.h"

#include <QTimer>

#include "interface/logic/ITreeViewController.h"

#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

struct TreeView::Impl final
	: private ITreeViewController::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Ui::TreeView ui{};
	TreeView & self;
	std::shared_ptr<ITreeViewController> controller;
	std::shared_ptr<ISettings> settings;

	Impl(TreeView & self
		, std::shared_ptr<ITreeViewController> controller
		, std::shared_ptr<ISettings> settings
	)
		: self(self)
		, controller(std::move(controller))
		, settings(std::move(settings))
	{
		ui.setupUi(&this->self);

		ui.cbView->setStyleSheet("QComboBox::drop-down {border-width: 0px;} QComboBox::down-arrow {image: url(noimg); border-width: 0px;}");
		for (const auto * name : this->controller->GetModeNames())
			ui.cbMode->addItem(QCoreApplication::translate(this->controller->TrContext(), name));

		OnModeChanged(this->controller->GetModeIndex());
		connect(ui.cbMode, &QComboBox::currentIndexChanged, [this] (const int index) { this->controller->SetModeIndex(index); });

		this->controller->RegisterObserver(this);
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
}

TreeView::~TreeView() = default;
