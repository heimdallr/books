#include "FillMenu.h"

#include <QAbstractItemView>
#include <QMenu>

#include <stack>

#include "interface/constants/Localization.h"
#include "interface/logic/ITreeViewController.h"
#include "logic/data/DataItem.h"

namespace HomeCompa::Flibrary {

void FillMenu(QMenu & menu, const IDataItem & item, ITreeViewController & controller, const QAbstractItemView & view)
{
	const auto font = menu.font();
	std::stack<std::pair<const IDataItem *, QMenu *>> stack { {{&item, &menu}} };
	while (!stack.empty())
	{
		auto [parent, subMenu] = stack.top();
		stack.pop();

		for (size_t i = 0, sz = parent->GetChildCount(); i < sz; ++i)
		{
			auto child = parent->GetChild(i);
			const auto enabledStr = child->GetData(MenuItem::Column::Enabled);
			const auto enabled = enabledStr.isEmpty() || QVariant(enabledStr).toBool();
			const auto title = child->GetData().toStdString();

			if (child->GetChildCount() != 0)
			{
				auto * subSubMenu = stack.emplace(child.get(), subMenu->addMenu(Loc::Tr(controller.TrContext(), title.data()))).second;
				subSubMenu->setFont(font);
				subSubMenu->setEnabled(enabled);
				continue;
			}

			if (const auto childId = child->GetData(MenuItem::Column::Id).toInt(); childId < 0)
			{
				subMenu->addSeparator();
				continue;
			}

			auto * action = subMenu->addAction(Loc::Tr(controller.TrContext(), title.data()), [&, child = std::move(child)] () mutable
			{
				controller.OnContextMenuTriggered(view.model(), view.currentIndex(), view.selectionModel()->selectedIndexes(), std::move(child));
			});

			action->setEnabled(enabled);
		}
	}
}

}
