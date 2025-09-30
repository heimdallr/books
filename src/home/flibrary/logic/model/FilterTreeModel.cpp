#include "FilterTreeModel.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

FilterTreeModel::FilterTreeModel(const std::shared_ptr<IModelProvider>& modelProvider, std::shared_ptr<IFilterController> controller, QObject* parent)
	: FilterModel(modelProvider, std::move(controller), parent)
{
	PLOGV << "FilterTreeModel created";
}

FilterTreeModel::~FilterTreeModel()
{
	PLOGV << "FilterTreeModel destroyed";
}

Qt::ItemFlags FilterTreeModel::flags(const QModelIndex& index) const
{
	auto flags = FilterModel::flags(index);
	if (index.column() == 0)
		return flags;

	if (const auto* item = GetInternalPointer(index); item && item->GetChildCount() == 0)
		flags |= Qt::ItemFlag::ItemIsUserCheckable | Qt::ItemFlag::ItemIsEditable;

	return flags;
}

std::vector<IDataItem::Ptr> FilterTreeModel::GetHideFilteredChanged(const std::unordered_set<QString>& ids) const
{
	std::vector<IDataItem::Ptr> changed;

	const auto enumerate = [&](const IDataItem::Ptr& item, const auto& r) -> void {
		if (item->GetChildCount() == 0)
		{
			if (ids.contains(item->GetId()) && !(item->GetFlags() & IDataItem::Flags::Filtered))
				changed.emplace_back(item);
			return;
		}

		for (size_t i = 0, sz = item->GetChildCount(); i < sz; ++i)
			r(item->GetChild(i), r);
	};
	enumerate(m_data, enumerate);

	return changed;
}

QVariant FilterTreeModel::IsChecked(const IDataItem& item, const IDataItem::Flags flags) const
{
	return item.GetChildCount() == 0 ? FilterModel::IsChecked(item, flags) : QVariant {};
}
