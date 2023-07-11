#include "TreeView.h"

#include <QTimer>

#include <plog/Log.h>

#include "ui_TreeView.h"

#include "interface/logic/ITreeViewController.h"

using namespace HomeCompa::Flibrary;

struct TreeView::Impl
{
	Ui::TreeView ui;
	std::shared_ptr<ITreeViewController> controller;

	Impl(std::shared_ptr<ITreeViewController> controller_)
		: controller(std::move(controller_))
	{
	}
};

TreeView::TreeView(std::shared_ptr<ITreeViewController> controller
	, QWidget * parent
)
	: QWidget(parent)
	, m_impl(std::move(controller))
{
	m_impl->ui.setupUi(this);
}

TreeView::~TreeView() = default;
