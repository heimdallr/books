#include "ModelUtil.h"

#include <queue>

namespace HomeCompa::Flibrary::ModelUtil
{

void EnumerateLeafs(const QAbstractItemModel& model, const QModelIndexList& indexList, const std::function<void(const QModelIndex&)>& f)
{
	std::queue<QModelIndex> queue;
	for (const auto& index : indexList)
		if (!index.isValid() || index.column() == 0)
			queue.push(index);

	while (!queue.empty())
	{
		const auto parent = queue.front();
		queue.pop();
		const auto rowCount = model.rowCount(parent);
		if (parent.isValid() && rowCount == 0)
			f(parent);

		for (int i = 0; i < rowCount; ++i)
			queue.push(model.index(i, 0, parent));
	}
}

} // namespace HomeCompa::Flibrary::ModelUtil
