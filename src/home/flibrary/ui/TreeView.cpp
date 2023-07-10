#include "TreeView.h"

#include "ui_TreeView.h"

using namespace HomeCompa::Flibrary;

struct TreeView::Impl
{
	Ui::TreeView ui;
};

TreeView::TreeView(QWidget * parent)
	: QWidget(parent)
{
	m_impl->ui.setupUi(this);
}

TreeView::~TreeView() = default;
