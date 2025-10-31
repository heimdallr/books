#include "TreeView.h"

#include <QHeaderView>

using namespace HomeCompa::Flibrary;

CustomTreeView::CustomTreeView(QWidget* parent)
	: QTreeView(parent)
{
}

void CustomTreeView::UpdateSectionSize()
{
	header()->setSectionResizeMode(1, QHeaderView::Interactive);
	if (const auto* model = this->model(); model && model->rowCount() > 0)
	{
		const auto height = rowHeight(model->index(0, 0));
		header()->resizeSection(1, height);
	}
}
