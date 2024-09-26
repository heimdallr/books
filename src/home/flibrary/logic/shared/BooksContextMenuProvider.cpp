#include "BooksContextMenuProvider.h"

#include <ranges>

#include <QString>
#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "database/interface/ITransaction.h"
#include "util/ISettings.h"

#include "interface/constants/SettingsConstant.h"
#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/logic/IScriptController.h"
#include "interface/ui/IUiFactory.h"

#include "ChangeNavigationController/GroupController.h"
#include "data/DataItem.h"
#include "data/DataProvider.h"
#include "database/DatabaseUser.h"
#include "extract/BooksExtractor.h"
#include "extract/InpxCollectionExtractor.h"
#include "shared/ReaderController.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT                        =                   "BookContextMenu";
constexpr auto READ_BOOK                      = QT_TRANSLATE_NOOP("BookContextMenu", "&Read");
constexpr auto EXPORT                         = QT_TRANSLATE_NOOP("BookContextMenu", "E&xport");
constexpr auto     SEND_AS_ARCHIVE            = QT_TRANSLATE_NOOP("BookContextMenu", "As &zip archive");
constexpr auto     SEND_AS_IS                 = QT_TRANSLATE_NOOP("BookContextMenu", "As &original format");
constexpr auto     SEND_AS_INPX               = QT_TRANSLATE_NOOP("BookContextMenu", "As &inpx collection");
constexpr auto GROUPS                         = QT_TRANSLATE_NOOP("BookContextMenu", "&Groups");
constexpr auto     GROUPS_ADD_TO              = QT_TRANSLATE_NOOP("BookContextMenu", "&Add to");
constexpr auto         GROUPS_ADD_TO_NEW      = QT_TRANSLATE_NOOP("BookContextMenu", "&New group...");
constexpr auto     GROUPS_REMOVE_FROM         = QT_TRANSLATE_NOOP("BookContextMenu", "&Remove from");
constexpr auto         GROUPS_REMOVE_FROM_ALL = QT_TRANSLATE_NOOP("BookContextMenu", "&All");
constexpr auto MY_RATE                        = QT_TRANSLATE_NOOP("BookContextMenu", "&My rate");
constexpr auto     REMOVE_MY_RATE             = QT_TRANSLATE_NOOP("BookContextMenu", "&Remove my rate");
constexpr auto CHECK                          = QT_TRANSLATE_NOOP("BookContextMenu", "&Check");
constexpr auto     CHECK_ALL                  = QT_TRANSLATE_NOOP("BookContextMenu", "&Check all");
constexpr auto     UNCHECK_ALL                = QT_TRANSLATE_NOOP("BookContextMenu", "&Uncheck all");
constexpr auto     INVERT_CHECK               = QT_TRANSLATE_NOOP("BookContextMenu", "&Invert");
constexpr auto TREE                           = QT_TRANSLATE_NOOP("BookContextMenu", "&Tree");
constexpr auto     TREE_COLLAPSE              = QT_TRANSLATE_NOOP("BookContextMenu", "C&ollapse");
constexpr auto     TREE_EXPAND                = QT_TRANSLATE_NOOP("BookContextMenu", "E&xpand");
constexpr auto     TREE_COLLAPSE_ALL          = QT_TRANSLATE_NOOP("BookContextMenu", "&Collapse all");
constexpr auto     TREE_EXPAND_ALL            = QT_TRANSLATE_NOOP("BookContextMenu", "&Expand all");
constexpr auto REMOVE_BOOK                    = QT_TRANSLATE_NOOP("BookContextMenu", "R&emove");
constexpr auto REMOVE_BOOK_UNDO               = QT_TRANSLATE_NOOP("BookContextMenu", "&Undo deletion");
constexpr auto SELECT_SEND_TO_FOLDER          = QT_TRANSLATE_NOOP("BookContextMenu", "Select destination folder");
TR_DEF

constexpr auto GROUPS_QUERY = "select g.GroupID, g.Title, coalesce(gl.BookID, -1) from Groups_User g left join Groups_List_User gl on gl.GroupID = g.GroupID and gl.BookID = ?";
constexpr auto USER_RATE_QUERY = "select coalesce(bu.UserRate, 0) from Books b left join Books_User bu on bu.BookID = b.BookID where b.BookID = ?";
constexpr auto DIALOG_KEY = "Export";

using GroupActionFunction = void (GroupController::*)(GroupController::Id id, GroupController::Ids ids, GroupController::Callback callback) const;

class IContextMenuHandler  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Function = void (IContextMenuHandler::*)(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, BooksContextMenuProvider::Callback callback) const;

public:
	virtual ~IContextMenuHandler() = default;

#define BOOKS_MENU_ACTION_ITEM(NAME) virtual void NAME(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, BooksContextMenuProvider::Callback callback) const = 0;
		BOOKS_MENU_ACTION_ITEMS_X_MACRO
#undef	BOOKS_MENU_ACTION_ITEM
};

constexpr std::pair<BooksMenuAction, IContextMenuHandler::Function> MENU_HANDLERS[]
{
	{ BooksMenuAction::AddToGroup, &IContextMenuHandler::AddToGroup },
#define BOOKS_MENU_ACTION_ITEM(NAME) { BooksMenuAction::NAME, &IContextMenuHandler::NAME },
		BOOKS_MENU_ACTION_ITEMS_X_MACRO
#undef	BOOKS_MENU_ACTION_ITEM
};

IDataItem::Ptr & Add(const IDataItem::Ptr & dst, QString title = {}, const BooksMenuAction id = BooksMenuAction::None)
{
	auto item = MenuItem::Create();
	item->SetData(std::move(title), MenuItem::Column::Title);
	item->SetData(QString::number(static_cast<int>(id)), MenuItem::Column::Id);
	return dst->AppendChild(std::move(item));
}

void CreateGroupMenu(const IDataItem::Ptr & root, const QString & id, DB::IDatabase & db)
{
	const auto parent = Add(root, Tr(GROUPS));

	const auto add = Add(parent, Tr(GROUPS_ADD_TO), BooksMenuAction::AddToGroup);
	const auto remove = Add(parent, Tr(GROUPS_REMOVE_FROM), BooksMenuAction::RemoveFromGroup);

	const auto query = db.CreateQuery(GROUPS_QUERY);
	query->Bind(0, id.toInt());
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto needAdd = query->Get<long long>(2) < 0;
		const auto & item = needAdd ? add : remove;
		Add(item, query->Get<const char *>(1), needAdd ? BooksMenuAction::AddToGroup : BooksMenuAction::RemoveFromGroup)
			->SetData(QString::number(query->Get<long long>(0)), MenuItem::Column::Parameter);
	}

	if (remove->GetChildCount() > 0)
	{
		Add(remove);
		Add(remove, Tr(GROUPS_REMOVE_FROM_ALL), BooksMenuAction::RemoveFromAllGroups)
			->SetData(QString::number(-1), MenuItem::Column::Parameter);
	}
	else
	{
		Add(remove);
		remove->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);
	}

	if (add->GetChildCount() > 0)
		Add(add);

	Add(add, Tr(GROUPS_ADD_TO_NEW), BooksMenuAction::AddToNewGroup)
		->SetData(QString::number(-1), MenuItem::Column::Parameter);
}

void CreateMyRateMenu(const IDataItem::Ptr & root, const QString & id, DB::IDatabase & db)
{
	const auto parent = Add(root, Tr(MY_RATE));
	for (int rate = 1; rate <= 5; ++rate)
		Add(parent, QString().assign(rate, QChar(9733)), BooksMenuAction::SetUserRate)->SetData(QString::number(rate), MenuItem::Column::Parameter);

	const auto query = db.CreateQuery(USER_RATE_QUERY);
	query->Bind(0, id.toInt());
	query->Execute();
	assert(!query->Eof());
	if (const auto currentUserRate = query->Get<int>(0); currentUserRate == 0)
		return;

	Add(parent)->SetData(QString::number(-1), MenuItem::Column::Parameter);
	Add(parent, Tr(REMOVE_MY_RATE), BooksMenuAction::SetUserRate)->SetData(QString::number(0), MenuItem::Column::Parameter);
}

void CreateSendMenu(const IDataItem::Ptr & root, const IScriptController::Scripts & scripts)
{
	const auto & send = Add(root, Tr(EXPORT));
	Add(send, Tr(SEND_AS_ARCHIVE), BooksMenuAction::SendAsArchive);
	Add(send, Tr(SEND_AS_IS), BooksMenuAction::SendAsIs);
	Add(send)->SetData(QString::number(-1), MenuItem::Column::Parameter);
	for (const auto & script : scripts)
	{
		const auto & scriptItem = Add(send, script.name, BooksMenuAction::SendAsScript);
		scriptItem->SetData(script.uid, MenuItem::Column::Parameter);
	}
	Add(send, Tr(SEND_AS_INPX), BooksMenuAction::SendAsInpx);
}

void CreateCheckMenu(const IDataItem::Ptr & root)
{
	const auto parent = Add(root, Tr(CHECK));
	Add(parent, Tr(CHECK_ALL), BooksMenuAction::CheckAll);
	Add(parent, Tr(UNCHECK_ALL), BooksMenuAction::UncheckAll);
	Add(parent, Tr(INVERT_CHECK), BooksMenuAction::InvertCheck);
}

void CreateTreeMenu(const IDataItem::Ptr & root, const ITreeViewController::RequestContextMenuOptions options)
{
	if (!!(options & ITreeViewController::RequestContextMenuOptions::IsTree))
		BooksContextMenuProvider::AddTreeMenuItems(Add(root, Tr(TREE)), options);
}

}

class BooksContextMenuProvider::Impl final
	: public IContextMenuHandler
{
public:
	explicit Impl(std::shared_ptr<const ISettings> settings
		, std::shared_ptr<DatabaseUser> databaseUser
		, const std::shared_ptr<const ILogicFactory>& logicFactory
		, std::shared_ptr<IUiFactory> uiFactory
		, std::shared_ptr<GroupController> groupController
		, std::shared_ptr<const DataProvider> dataProvider
		, std::shared_ptr<IScriptController> scriptController
	)
		: m_settings(std::move(settings))
		, m_databaseUser(std::move(databaseUser))
		, m_logicFactory(logicFactory)
		, m_uiFactory(std::move(uiFactory))
		, m_groupController(std::move(groupController))
		, m_dataProvider(std::move(dataProvider))
		, m_scriptController(std::move(scriptController))
	{
	}

public:
	void Request(const QModelIndex & index, const ITreeViewController::RequestContextMenuOptions options, Callback callback)
	{
		auto scripts = m_scriptController->GetScripts();
		std::ranges::sort(scripts, [] (const auto & lhs, const auto & rhs) { return lhs.number < rhs.number; });
		m_databaseUser->Execute({ "Create context menu", [
			  id = index.data(Role::Id).toString()
			, type = index.data(Role::Type).value<ItemType>()
			, removed = index.data(Role::IsRemoved).toBool()
			, options
			, callback = std::move(callback)
			, db = m_databaseUser->Database()
			, scripts = std::move(scripts)
		]() mutable
		{
			auto result = MenuItem::Create();

			if (type == ItemType::Books)
				Add(result, Tr(READ_BOOK), BooksMenuAction::ReadBook);

			CreateSendMenu(result, scripts);

			if (type == ItemType::Books)
			{
				CreateGroupMenu(result, id, *db);
				CreateMyRateMenu(result, id, *db);
			}

			CreateCheckMenu(result);
			CreateTreeMenu(result, options);

			if (type == ItemType::Books)
				Add(result, Tr(removed ? REMOVE_BOOK_UNDO : REMOVE_BOOK), removed ? BooksMenuAction::UndoRemoveBook : BooksMenuAction::RemoveBook);

			return [callback = std::move(callback), result = std::move(result)] (size_t)
			{
				if (result->GetChildCount() > 0)
					callback(result);
			};
		} });
	}

private: // IContextMenuHandler
	void ReadBook(QAbstractItemModel * /*model*/, const QModelIndex & index, const QList<QModelIndex> & /*indexList*/, IDataItem::Ptr item, Callback callback) const override
	{
		auto readerController = ILogicFactory::Lock(m_logicFactory)->CreateReaderController();
		readerController->Read(index.data(Role::Folder).toString(), index.data(Role::FileName).toString(), [readerController, item = std::move(item), callback = std::move(callback)] () mutable
		{
			callback(item);
			readerController.reset();
		});
	}

	void RemoveBook(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		ChangeBookRemoved(model, index, indexList, std::move(item), std::move(callback), true);
	}

	void UndoRemoveBook(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		ChangeBookRemoved(model, index, indexList, std::move(item), std::move(callback), false);
	}

	void CheckAll(QAbstractItemModel * model, const QModelIndex &, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		model->setData({}, QVariant::fromValue(indexList), Role::CheckAll);
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void UncheckAll(QAbstractItemModel * model, const QModelIndex &, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		model->setData({}, QVariant::fromValue(indexList), Role::UncheckAll);
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void InvertCheck(QAbstractItemModel * model, const QModelIndex &, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		model->setData({}, QVariant::fromValue(indexList), Role::InvertCheck);
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void Collapse(QAbstractItemModel *, const QModelIndex &, const QList<QModelIndex> &, IDataItem::Ptr item, Callback callback) const override
	{
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void Expand(QAbstractItemModel *, const QModelIndex &, const QList<QModelIndex> &, IDataItem::Ptr item, Callback callback) const override
	{
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void CollapseAll(QAbstractItemModel *, const QModelIndex &, const QList<QModelIndex> &, IDataItem::Ptr item, Callback callback) const override
	{
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void ExpandAll(QAbstractItemModel *, const QModelIndex &, const QList<QModelIndex> &, IDataItem::Ptr item, Callback callback) const override
	{
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void AddToNewGroup(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		GroupAction(model, index, indexList, std::move(item), std::move(callback), &GroupController::AddToGroup);
	}

	void AddToGroup(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		GroupAction(model, index, indexList, std::move(item), std::move(callback), &GroupController::AddToGroup);
	}

	void RemoveFromGroup(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		GroupAction(model, index, indexList, std::move(item), std::move(callback), &GroupController::RemoveFromGroup);
	}

	void RemoveFromAllGroups(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		GroupAction(model, index, indexList, std::move(item), std::move(callback), &GroupController::RemoveFromGroup);
	}

	void SetUserRate(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		m_databaseUser->Execute({ "Set my rate", [this, ids = GetSelected(model, index, indexList), item = std::move(item), callback = std::move(callback)] () mutable
		{
			const auto db = m_databaseUser->Database();
			const auto transaction = db->CreateTransaction();
			const auto command = transaction->CreateCommand("insert or replace into Books_User(BookID, UserRate) values(:id, :user_rate)");
			const auto rate = [&]
			{
				const auto str = item->GetData(MenuItem::Column::Parameter);
				assert(!str.isEmpty());
				bool ok = false;
				const auto value = str.toInt(&ok);
				return ok ? value : 0;
			}();

			for (const auto & id : ids)
			{
				command->Bind(":id", id);
				if (rate)
					command->Bind(":user_rate", rate);
				else
					command->Bind(":user_rate");
				command->Execute();
			}
			transaction->Commit();

			return [this, item = std::move(item), callback = std::move(callback)] (size_t)
			{
				callback(item);
				m_dataProvider->RequestBooks(true);
			};
		} });
	}

	void SendAsArchive(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		SendAsImpl(model, index, indexList, std::move(item), std::move(callback), &BooksExtractor::ExtractAsArchives);
	}

	void SendAsIs(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		SendAsImpl(model, index, indexList, std::move(item), std::move(callback), &BooksExtractor::ExtractAsIs);
	}

	void SendAsInpx(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		auto idList = ILogicFactory::Lock(m_logicFactory)->GetSelectedBookIds(model, index, indexList, { Role::Id });
		if (idList.empty())
			return;

		std::transform(std::next(idList.begin()), idList.end(), std::back_inserter(idList.front()), [] (auto & id)
		{
			return std::move(id.front());
		});
		auto dir = m_uiFactory->GetExistingDirectory(DIALOG_KEY, SELECT_SEND_TO_FOLDER);
		if (dir.isEmpty())
			return callback(item);

		auto extractor = ILogicFactory::Lock(m_logicFactory)->CreateInpxCollectionExtractor();
		extractor->ExtractAsInpxCollection(std::move(dir), idList.front(), *m_dataProvider, [extractor, item = std::move(item), callback = std::move(callback)] (const bool hasError) mutable
		{
			item->SetData(QString::number(hasError), MenuItem::Column::HasError);
			callback(item);
			extractor.reset();
		});
	}

	void SendAsScript(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		const auto commands = m_scriptController->GetCommands(item->GetData(MenuItem::Column::Parameter));
		const auto hasUserDestinationFolder = std::ranges::any_of(commands, [] (const auto & command) { return IScriptController::HasMacro(command.args, IScriptController::Macro::UserDestinationFolder); });
		Send(model, index, indexList, std::move(item), std::move(callback), &BooksExtractor::ExtractAsScript
			, QString("%1/%2")
				.arg(IScriptController::GetMacro(IScriptController::Macro::UserDestinationFolder))
				.arg(IScriptController::GetMacro(IScriptController::Macro::FileName))
			, hasUserDestinationFolder);
	}

private:
	void SendAsImpl(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback, const BooksExtractor::Extract f) const
	{
		auto outputFileNameTemplate = m_settings->Get(Constant::Settings::EXPORT_TEMPLATE_KEY, IScriptController::GetDefaultOutputFileNameTemplate());
		const bool dstFolderRequired = IScriptController::HasMacro(outputFileNameTemplate, IScriptController::Macro::UserDestinationFolder);
		Send(model, index, indexList, std::move(item), std::move(callback), f, std::move(outputFileNameTemplate), dstFolderRequired);
	}

	void Send(QAbstractItemModel * model
		, const QModelIndex & index
		, const QList<QModelIndex> & indexList
		, IDataItem::Ptr item
		, Callback callback
		, const BooksExtractor::Extract f
		, QString outputFileNameTemplate
		, const bool dstFolderRequired
	) const
	{
		auto dir = dstFolderRequired ? m_uiFactory->GetExistingDirectory(DIALOG_KEY, SELECT_SEND_TO_FOLDER) : QString();
		if (dstFolderRequired && dir.isEmpty())
			return callback(item);

		const auto logicFactory = ILogicFactory::Lock(m_logicFactory);
		auto books = logicFactory->GetExtractedBooks(model, index, indexList);
		auto extractor = logicFactory->CreateBooksExtractor();
		const auto parameter = item->GetData(MenuItem::Column::Parameter);
		((*extractor).*f)(std::move(dir), parameter, std::move(books), std::move(outputFileNameTemplate), [extractor, item = std::move(item), callback = std::move(callback)] (const bool hasError) mutable
		{
			item->SetData(QString::number(hasError), MenuItem::Column::HasError);
			callback(item);
			extractor.reset();
		});
	}

	void GroupAction(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback, const GroupActionFunction & f) const
	{
		const auto id = item->GetData(MenuItem::Column::Parameter).toLongLong();
		((*m_groupController).*f)(id, GetSelected(model, index, indexList), [item = std::move(item), callback = std::move(callback)]
		{
			callback(item);
		});
	}

	GroupController::Ids GetSelected(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList) const
	{
		auto selected = ILogicFactory::Lock(m_logicFactory)->GetSelectedBookIds(model, index, indexList, { Role::Id });

		GroupController::Ids ids;
		ids.reserve(selected.size());
		std::ranges::transform(selected, std::inserter(ids, ids.end()), [] (const auto & item) { return item.front().toLongLong(); });
		return ids;
	}

	void ChangeBookRemoved(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback, const bool remove) const
	{
		m_databaseUser->Execute({ "Remove books", [this, ids = GetSelected(model, index, indexList), item = std::move(item), callback = std::move(callback), remove] () mutable
		{
			const auto db = m_databaseUser->Database();
			const auto transaction = db->CreateTransaction();
			const auto command = transaction->CreateCommand("insert or replace into Books_User(BookID, IsDeleted) values(:id, :is_deleted)");
			for (const auto & id : ids)
			{
				command->Bind(":id", id);
				command->Bind(":is_deleted", remove ? 1 : 0);
				command->Execute();
			}
			transaction->Commit();

			return [this, item = std::move(item), callback = std::move(callback)] (size_t)
			{
				callback(item);
				m_dataProvider->RequestBooks(true);
			};
		} });
	}

private:
	std::shared_ptr<const ISettings> m_settings;
	PropagateConstPtr<DatabaseUser, std::shared_ptr> m_databaseUser;
	std::weak_ptr<const ILogicFactory> m_logicFactory;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	PropagateConstPtr<GroupController, std::shared_ptr> m_groupController;
	std::shared_ptr<const DataProvider> m_dataProvider;
	std::shared_ptr<IScriptController> m_scriptController;
};

void BooksContextMenuProvider::AddTreeMenuItems(const IDataItem::Ptr & parent, const ITreeViewController::RequestContextMenuOptions options)
{
	if (!!(options & ITreeViewController::RequestContextMenuOptions::NodeCollapsed))
		Add(parent, Tr(TREE_EXPAND), BooksMenuAction::Expand);
	if (!!(options & ITreeViewController::RequestContextMenuOptions::NodeExpanded))
		Add(parent, Tr(TREE_COLLAPSE), BooksMenuAction::Collapse);
	if (const auto node = Add(parent, Tr(TREE_COLLAPSE_ALL), BooksMenuAction::CollapseAll); !(options & ITreeViewController::RequestContextMenuOptions::HasExpanded))
		node->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);
	if (const auto node = Add(parent, Tr(TREE_EXPAND_ALL), BooksMenuAction::ExpandAll); !(options & ITreeViewController::RequestContextMenuOptions::HasCollapsed))
		node->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);
}

BooksContextMenuProvider::BooksContextMenuProvider(std::shared_ptr<const ISettings> settings
	, std::shared_ptr<DatabaseUser> databaseUser
	, const std::shared_ptr<const ILogicFactory>& logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
	, std::shared_ptr<GroupController> groupController
	, std::shared_ptr<DataProvider> dataProvider
	, std::shared_ptr<IScriptController> scriptController
)
	: m_impl(std::move(settings)
		, std::move(databaseUser)
		, logicFactory
		, std::move(uiFactory)
		, std::move(groupController)
		, std::move(dataProvider)
		, std::move(scriptController)
	)
{
	PLOGD << "BooksContextMenuProvider created";
}

BooksContextMenuProvider::~BooksContextMenuProvider()
{
	PLOGD << "BooksContextMenuProvider destroyed";
}

void BooksContextMenuProvider::Request(const QModelIndex & index, const ITreeViewController::RequestContextMenuOptions options, Callback callback)
{
	m_impl->Request(index, options, std::move(callback));
}

void BooksContextMenuProvider::OnContextMenuTriggered(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const
{
	const auto invoker = FindSecond(MENU_HANDLERS, static_cast<BooksMenuAction>(item->GetData(MenuItem::Column::Id).toInt()));
	std::invoke(invoker, *m_impl, model, std::cref(index), std::cref(indexList), std::move(item), std::move(callback));
}
