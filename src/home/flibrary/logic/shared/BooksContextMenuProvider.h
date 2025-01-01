#pragma once

#include <functional>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/logic/IDataItem.h"
#include "interface/logic/ITreeViewController.h"

class QAbstractItemModel;
class QModelIndex;

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class BooksContextMenuProvider final
{
	NON_COPY_MOVABLE(BooksContextMenuProvider)

public:
	using Callback = std::function<void(const IDataItem::Ptr &)>;
	static void AddTreeMenuItems(const IDataItem::Ptr & root, ITreeViewController::RequestContextMenuOptions options);

public:
	BooksContextMenuProvider(std::shared_ptr<const ISettings> settings
		, std::shared_ptr<class IDatabaseUser> databaseUser
		, const std::shared_ptr<const class ILogicFactory>& logicFactory
		, std::shared_ptr<class IUiFactory> uiFactory
		, std::shared_ptr<class GroupController> groupController
		, std::shared_ptr<class DataProvider> dataProvider
		, std::shared_ptr<class IScriptController> scriptController
	);
	~BooksContextMenuProvider();

public:
	void Request(const QModelIndex & index, ITreeViewController::RequestContextMenuOptions options, Callback callback);
	void OnContextMenuTriggered(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
