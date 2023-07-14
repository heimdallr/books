#include "TreeView.h"

#include <QTimer>

#include <plog/Log.h>

#include "ui_TreeView.h"

#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ITreeViewController.h"
#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

struct TreeView::Impl
{
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
			ui.cbMode->addItem(QCoreApplication::translate(this->controller->GetTreeViewName(), name), name);

		connect(ui.cbMode, &QComboBox::currentIndexChanged, [this] (int)
		{
			this->settings->Set(QString(Constant::Settings::VIEW_MODE_KEY_TEMPLATE).arg(this->controller->GetTreeViewName()), ui.cbMode->currentData());
		});
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
