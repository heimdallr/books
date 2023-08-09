#pragma once

#include <functional>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/logic/IDataItem.h"

class QAbstractItemModel;
class QModelIndex;

namespace HomeCompa::Flibrary {

class BooksContextMenuProvider final
{
	NON_COPY_MOVABLE(BooksContextMenuProvider)

public:
	using Callback = std::function<void(const IDataItem::Ptr &)>;

public:
	BooksContextMenuProvider(std::shared_ptr<class DatabaseUser> databaseUser
		, std::shared_ptr<class GroupController> groupController
		, std::shared_ptr<class DataProvider> dataProvider
	);
	~BooksContextMenuProvider();

public:
	void Request(const QModelIndex & index, Callback callback);
	void OnContextMenuTriggered(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
