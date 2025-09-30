#pragma once

#include <functional>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDataItem.h"
#include "interface/logic/IDataProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IReaderController.h"
#include "interface/logic/IScriptController.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/ui/IUiFactory.h"

#include "util/ISettings.h"

class QAbstractItemModel;
class QModelIndex;

namespace HomeCompa::Flibrary
{

class BooksContextMenuProvider final
{
	NON_COPY_MOVABLE(BooksContextMenuProvider)

public:
	using Callback = std::function<void(const IDataItem::Ptr&)>;
	static void AddTreeMenuItems(const IDataItem::Ptr& parent, ITreeViewController::RequestContextMenuOptions options);

public:
	BooksContextMenuProvider(
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<const ISettings>            settings,
		std::shared_ptr<const IReaderController>    readerController,
		std::shared_ptr<const IDatabaseUser>        databaseUser,
		std::shared_ptr<const IBookInfoProvider>    dataProvider,
		std::shared_ptr<const IUiFactory>           uiFactory,
		std::shared_ptr<IScriptController>          scriptController
	);
	~BooksContextMenuProvider();

public:
	void Request(const QModelIndex& index, ITreeViewController::RequestContextMenuOptions options, Callback callback);
	void OnContextMenuTriggered(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
