#include "BooksContextMenuProvider.h"

#include <ranges>

#include <QTimer>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ICollectionCleaner.h"
#include "interface/logic/IInpxGenerator.h"

#include "ChangeNavigationController/GroupController.h"
#include "data/DataItem.h"
#include "database/DatabaseUtil.h"
#include "extract/BooksExtractor.h"
#include "util/localization.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "BookContextMenu";
constexpr auto READ_BOOK = QT_TRANSLATE_NOOP("BookContextMenu", "&Read");
constexpr auto EXPORT = QT_TRANSLATE_NOOP("BookContextMenu", "E&xport");
constexpr auto SEND_AS_ARCHIVE = QT_TRANSLATE_NOOP("BookContextMenu", "As &zip archive");
constexpr auto SEND_AS_IS = QT_TRANSLATE_NOOP("BookContextMenu", "As &original format");
constexpr auto UNPACK = QT_TRANSLATE_NOOP("BookContextMenu", "&Unpack");
constexpr auto SEND_AS_INPX = QT_TRANSLATE_NOOP("BookContextMenu", "As &inpx collection");
constexpr auto SEND_AS_SINGLE_INPX = QT_TRANSLATE_NOOP("BookContextMenu", "Generate inde&x file (*.inpx)");
constexpr auto GROUPS = QT_TRANSLATE_NOOP("BookContextMenu", "&Groups");
constexpr auto GROUPS_ADD_TO = QT_TRANSLATE_NOOP("BookContextMenu", "&Add to");
constexpr auto GROUPS_ADD_TO_NEW = QT_TRANSLATE_NOOP("BookContextMenu", "&New group...");
constexpr auto GROUPS_REMOVE_FROM = QT_TRANSLATE_NOOP("BookContextMenu", "&Remove from");
constexpr auto GROUPS_REMOVE_FROM_ALL = QT_TRANSLATE_NOOP("BookContextMenu", "&All");
constexpr auto MY_RATE = QT_TRANSLATE_NOOP("BookContextMenu", "&My rate");
constexpr auto REMOVE_MY_RATE = QT_TRANSLATE_NOOP("BookContextMenu", "&Remove my rate");
constexpr auto CHECK = QT_TRANSLATE_NOOP("BookContextMenu", "&Check");
constexpr auto CHECK_ALL = QT_TRANSLATE_NOOP("BookContextMenu", "&Check all");
constexpr auto UNCHECK_ALL = QT_TRANSLATE_NOOP("BookContextMenu", "&Uncheck all");
constexpr auto INVERT_CHECK = QT_TRANSLATE_NOOP("BookContextMenu", "&Invert");
constexpr auto TREE = QT_TRANSLATE_NOOP("BookContextMenu", "&Tree");
constexpr auto TREE_COLLAPSE = QT_TRANSLATE_NOOP("BookContextMenu", "C&ollapse");
constexpr auto TREE_EXPAND = QT_TRANSLATE_NOOP("BookContextMenu", "E&xpand");
constexpr auto TREE_COLLAPSE_ALL = QT_TRANSLATE_NOOP("BookContextMenu", "&Collapse all");
constexpr auto TREE_EXPAND_ALL = QT_TRANSLATE_NOOP("BookContextMenu", "&Expand all");
constexpr auto REMOVE_BOOK = QT_TRANSLATE_NOOP("BookContextMenu", "R&emove");
constexpr auto REMOVE_BOOK_UNDO = QT_TRANSLATE_NOOP("BookContextMenu", "&Undo deletion");
constexpr auto REMOVE_BOOK_FROM_ARCHIVE = QT_TRANSLATE_NOOP("BookContextMenu", "&Delete permanently");
constexpr auto SELECT_SEND_TO_FOLDER = QT_TRANSLATE_NOOP("BookContextMenu", "Select destination folder");
constexpr auto CHANGE_LANGUAGE = QT_TRANSLATE_NOOP("BookContextMenu", "Change language");

constexpr auto CANNOT_SET_USER_RATE = QT_TRANSLATE_NOOP("BookContextMenu", "Cannot set rate");
constexpr auto CANNOT_SET_LANGUAGE = QT_TRANSLATE_NOOP("BookContextMenu", "Cannot set language of books");
constexpr auto CANNOT_REMOVE_BOOK = QT_TRANSLATE_NOOP("BookContextMenu", "Books %1 failed");
constexpr auto REMOVE = QT_TRANSLATE_NOOP("BookContextMenu", "removing");
constexpr auto RESTORE = QT_TRANSLATE_NOOP("BookContextMenu", "restoring");

constexpr auto REMOVE_PERMANENTLY_CONFIRM = QT_TRANSLATE_NOOP("BookContextMenu", "The result of this operation cannot be undone. Are you sure you want to delete the books permanently?");
constexpr auto CHANGE_LANGUAGE_CONFIRM = QT_TRANSLATE_NOOP("BookContextMenu", "Are you sure you want to change the language of the books to %1?");

TR_DEF

constexpr auto GROUPS_QUERY = "select g.GroupID, g.Title, coalesce(gl.BookID, -1) from Groups_User g left join Groups_List_User_View gl on gl.GroupID = g.GroupID and gl.BookID = ?";
constexpr auto USER_RATE_QUERY = "select coalesce(bu.UserRate, 0) from Books b left join Books_User bu on bu.BookID = b.BookID where b.BookID = ?";

using GroupActionFunction = void (GroupController::*)(GroupController::Id id, GroupController::Ids ids, GroupController::Callback callback) const;

class IContextMenuHandler // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Function =
		void (IContextMenuHandler::*)(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, BooksContextMenuProvider::Callback callback) const;

public:
	virtual ~IContextMenuHandler() = default;

#define BOOKS_MENU_ACTION_ITEM(NAME) \
	virtual void NAME(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, BooksContextMenuProvider::Callback callback) const = 0;
	BOOKS_MENU_ACTION_ITEMS_X_MACRO
#undef BOOKS_MENU_ACTION_ITEM
};

constexpr std::pair<BooksMenuAction, IContextMenuHandler::Function> MENU_HANDLERS[] {
#define BOOKS_MENU_ACTION_ITEM(NAME) { BooksMenuAction::NAME, &IContextMenuHandler::NAME },
	BOOKS_MENU_ACTION_ITEMS_X_MACRO
#undef BOOKS_MENU_ACTION_ITEM
};

IDataItem::Ptr& Add(const IDataItem::Ptr& dst, QString title = {}, const BooksMenuAction id = BooksMenuAction::None)
{
	auto item = MenuItem::Create();
	item->SetData(std::move(title), MenuItem::Column::Title);
	item->SetData(QString::number(static_cast<int>(id)), MenuItem::Column::Id);
	return dst->AppendChild(std::move(item));
}

void CreateGroupMenu(const IDataItem::Ptr& root, const QString& id, DB::IDatabase& db)
{
	const auto parent = Add(root, Tr(GROUPS));

	const auto add = Add(parent, Tr(GROUPS_ADD_TO), BooksMenuAction::AddToGroup);
	const auto remove = Add(parent, Tr(GROUPS_REMOVE_FROM), BooksMenuAction::RemoveFromGroup);

	const auto query = db.CreateQuery(GROUPS_QUERY);
	query->Bind(0, id.toInt());
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto needAdd = query->Get<long long>(2) < 0;
		const auto& item = needAdd ? add : remove;
		Add(item, query->Get<const char*>(1), needAdd ? BooksMenuAction::AddToGroup : BooksMenuAction::RemoveFromGroup)->SetData(QString::number(query->Get<long long>(0)), MenuItem::Column::Parameter);
	}

	if (remove->GetChildCount() > 0)
	{
		Add(remove);
		Add(remove, Tr(GROUPS_REMOVE_FROM_ALL), BooksMenuAction::RemoveFromAllGroups)->SetData(QString::number(-1), MenuItem::Column::Parameter);
	}
	else
	{
		Add(remove);
		remove->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);
	}

	if (add->GetChildCount() > 0)
		Add(add);

	Add(add, Tr(GROUPS_ADD_TO_NEW), BooksMenuAction::AddToNewGroup)->SetData(QString::number(-1), MenuItem::Column::Parameter);
}

void CreateMyRateMenu(const IDataItem::Ptr& root, const QString& id, DB::IDatabase& db, const int starSymbol)
{
	const auto parent = Add(root, Tr(MY_RATE));
	for (int rate = 1; rate <= 5; ++rate)
		Add(parent, QString(rate, QChar(starSymbol)), BooksMenuAction::SetUserRate)->SetData(QString::number(rate), MenuItem::Column::Parameter);

	const auto query = db.CreateQuery(USER_RATE_QUERY);
	query->Bind(0, id.toInt());
	query->Execute();
	assert(!query->Eof());
	if (const auto currentUserRate = query->Get<int>(0); currentUserRate == 0)
		return;

	Add(parent)->SetData(QString::number(-1), MenuItem::Column::Parameter);
	Add(parent, Tr(REMOVE_MY_RATE), BooksMenuAction::SetUserRate)->SetData(QString::number(0), MenuItem::Column::Parameter);
}

void CreateSendMenu(const IDataItem::Ptr& root, const ITreeViewController::RequestContextMenuOptions options, const IScriptController::Scripts& scripts)
{
	const auto& send = Add(root, Tr(EXPORT));
	Add(send, Tr(SEND_AS_ARCHIVE), BooksMenuAction::SendAsArchive);
	Add(send, Tr(SEND_AS_IS), BooksMenuAction::SendAsIs);
	if (!!(options & ITreeViewController::RequestContextMenuOptions::IsArchive))
		Add(send, Tr(UNPACK), BooksMenuAction::SendUnpack);

	if (!scripts.empty())
	{
		Add(send)->SetData(QString::number(-1), MenuItem::Column::Parameter);
		for (const auto& script : scripts)
		{
			const auto& scriptItem = Add(send, script.name, BooksMenuAction::SendAsScript);
			scriptItem->SetData(script.uid, MenuItem::Column::Parameter);
		}
	}
	Add(send)->SetData(QString::number(-1), MenuItem::Column::Parameter);
	Add(send, Tr(SEND_AS_INPX), BooksMenuAction::SendAsInpxCollection);
	Add(send, Tr(SEND_AS_SINGLE_INPX), BooksMenuAction::SendAsInpxFile);
}

void CreateCheckMenu(const IDataItem::Ptr& root)
{
	const auto parent = Add(root, Tr(CHECK));
	Add(parent, Tr(CHECK_ALL), BooksMenuAction::CheckAll);
	Add(parent, Tr(UNCHECK_ALL), BooksMenuAction::UncheckAll);
	Add(parent, Tr(INVERT_CHECK), BooksMenuAction::InvertCheck);
}

void CreateChangeLangMenu(const IDataItem::Ptr& root, const QString& currentLocale)
{
	const auto parent = Add(root, Tr(CHANGE_LANGUAGE));
	std::vector<std::tuple<const char*, QString, int>> languages {
		{ "-1", "", 5000 }
	};
	languages.reserve(std::size(LANGUAGES) + 1);
	std::ranges::transform(LANGUAGES, std::back_inserter(languages), [](const Language& item) { return std::make_tuple(item.key, Loc::Tr(LANGUAGES_CONTEXT, item.title), item.priority); });
	if (auto it = std::ranges::find(languages, currentLocale, [](const auto& item) { return std::get<0>(item); }); it != languages.end())
	{
		auto& item = *it;
		std::get<2>(item) = std::numeric_limits<int>::min();
	}
	std::ranges::sort(languages, {}, [](const auto& item) { return std::make_tuple(std::get<2>(item), std::get<1>(item)); });

	for (auto&& [key, value, priority] : languages)
		Add(parent, std::move(value), priority == 5000 ? BooksMenuAction::None : BooksMenuAction::ChangeLanguage)->SetData(key, MenuItem::Column::Parameter);
}

void CreateTreeMenu(const IDataItem::Ptr& root, const ITreeViewController::RequestContextMenuOptions options)
{
	if (!!(options & ITreeViewController::RequestContextMenuOptions::IsTree))
		BooksContextMenuProvider::AddTreeMenuItems(Add(root, Tr(TREE)), options);
}

} // namespace

class BooksContextMenuProvider::Impl final : public IContextMenuHandler
{
public:
	explicit Impl(const std::shared_ptr<const ILogicFactory>& logicFactory,
	              std::shared_ptr<const ISettings> settings,
	              std::shared_ptr<const IReaderController> readerController,
	              std::shared_ptr<const IDatabaseUser> databaseUser,
	              std::shared_ptr<const IBookInfoProvider> dataProvider,
	              std::shared_ptr<const IUiFactory> uiFactory,
	              std::shared_ptr<GroupController> groupController,
	              std::shared_ptr<IScriptController> scriptController)
		: m_logicFactory { logicFactory }
		, m_settings { std::move(settings) }
		, m_readerController { std::move(readerController) }
		, m_databaseUser { std::move(databaseUser) }
		, m_dataProvider { std::move(dataProvider) }
		, m_uiFactory { std::move(uiFactory) }
		, m_groupController { std::move(groupController) }
		, m_scriptController { std::move(scriptController) }
	{
	}

public:
	void Request(const QModelIndex& index, const ITreeViewController::RequestContextMenuOptions options, Callback callback) const
	{
		auto scripts = m_scriptController->GetScripts();
		std::ranges::sort(scripts, [](const auto& lhs, const auto& rhs) { return lhs.number < rhs.number; });
		auto currentLocale = Loc::GetLocale(*m_settings);

		m_databaseUser->Execute({ "Create context menu",
		                          [id = index.data(Role::Id).toString(),
		                           type = index.data(Role::Type).value<ItemType>(),
		                           removed = index.data(Role::IsRemoved).toBool(),
		                           options,
		                           starSymbol = m_starSymbol,
		                           callback = std::move(callback),
		                           db = m_databaseUser->Database(),
		                           scripts = std::move(scripts),
		                           currentLocale = std::move(currentLocale)]() mutable
		                          {
									  auto result = MenuItem::Create();

									  if (type == ItemType::Books)
										  Add(result, Tr(READ_BOOK), BooksMenuAction::ReadBook);

									  CreateSendMenu(result, options, scripts);
									  Add(result)->SetData(QString::number(-1), MenuItem::Column::Parameter);

									  if (type == ItemType::Books)
									  {
										  CreateGroupMenu(result, id, *db);
										  CreateMyRateMenu(result, id, *db, starSymbol);
									  }

									  CreateCheckMenu(result);
									  CreateTreeMenu(result, options);
									  CreateChangeLangMenu(result, currentLocale);

									  if (type == ItemType::Books)
									  {
										  Add(result)->SetData(QString::number(-1), MenuItem::Column::Parameter);
										  Add(result, Tr(removed ? REMOVE_BOOK_UNDO : REMOVE_BOOK), removed ? BooksMenuAction::UndoRemoveBook : BooksMenuAction::RemoveBook);
										  auto removeItem = Add(result, Tr(REMOVE_BOOK_FROM_ARCHIVE), BooksMenuAction::RemoveBookFromArchive);
										  if (!(options & ITreeViewController::RequestContextMenuOptions::AllowDestructiveOperations))
											  removeItem->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);
									  }

									  return [callback = std::move(callback), result = std::move(result)](size_t)
									  {
										  if (result->GetChildCount() > 0)
											  callback(result);
									  };
								  } });
	}

private: // IContextMenuHandler
	void ReadBook(QAbstractItemModel* /*model*/, const QModelIndex& index, const QList<QModelIndex>& /*indexList*/, IDataItem::Ptr item, Callback callback) const override
	{
		m_readerController->Read(index.data(Role::Folder).toString(), index.data(Role::FileName).toString(), [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void RemoveBook(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		ChangeBookRemoved(model, index, indexList, std::move(item), std::move(callback), true);
	}

	void RemoveBookFromArchive(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		if (m_uiFactory->ShowQuestion(Tr(REMOVE_PERMANENTLY_CONFIRM), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;

		auto idList = ILogicFactory::Lock(m_logicFactory)->GetSelectedBookIds(model, index, indexList, { Role::Id, Role::Folder, Role::FileName });
		if (idList.empty())
			return;

		ICollectionCleaner::Books books;
		books.reserve(idList.size());
		const auto count = idList.size();
		std::ranges::transform(std::move(idList),
		                       std::back_inserter(books),
		                       [](auto&& idListItem) { return ICollectionCleaner::Book { idListItem[0].toLongLong(), std::move(idListItem[1]), std::move(idListItem[2]) }; });
		auto cleaner = ILogicFactory::Lock(m_logicFactory)->CreateCollectionCleaner();
		auto& cleanerRef = *cleaner;
		cleanerRef.RemovePermanently(std::move(books),
		                             [&, cleaner = std::move(cleaner), item = std::move(item), callback = std::move(callback), count](const bool result) mutable
		                             {
										 if (result)
											 m_uiFactory->ShowInfo(Loc::Tr(ICollectionCleaner::CONTEXT, ICollectionCleaner::REMOVE_PERMANENTLY_INFO).arg(count));
										 callback(item);
									 });
	}

	void UndoRemoveBook(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		ChangeBookRemoved(model, index, indexList, std::move(item), std::move(callback), false);
	}

	void CheckAll(QAbstractItemModel* model, const QModelIndex&, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		model->setData({}, QVariant::fromValue(indexList), Role::CheckAll);
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void UncheckAll(QAbstractItemModel* model, const QModelIndex&, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		model->setData({}, QVariant::fromValue(indexList), Role::UncheckAll);
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void InvertCheck(QAbstractItemModel* model, const QModelIndex&, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		model->setData({}, QVariant::fromValue(indexList), Role::InvertCheck);
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void Collapse(QAbstractItemModel*, const QModelIndex&, const QList<QModelIndex>&, IDataItem::Ptr item, Callback callback) const override
	{
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void Expand(QAbstractItemModel*, const QModelIndex&, const QList<QModelIndex>&, IDataItem::Ptr item, Callback callback) const override
	{
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void CollapseAll(QAbstractItemModel*, const QModelIndex&, const QList<QModelIndex>&, IDataItem::Ptr item, Callback callback) const override
	{
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void ExpandAll(QAbstractItemModel*, const QModelIndex&, const QList<QModelIndex>&, IDataItem::Ptr item, Callback callback) const override
	{
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] { callback(item); });
	}

	void AddToNewGroup(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		AddToGroup(model, index, indexList, std::move(item), std::move(callback));
	}

	void AddToGroup(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		GroupAction(model, index, indexList, std::move(item), std::move(callback), &GroupController::AddToGroup);
	}

	void RemoveFromGroup(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		GroupAction(model, index, indexList, std::move(item), std::move(callback), &GroupController::RemoveFromGroup);
	}

	void RemoveFromAllGroups(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		RemoveFromGroup(model, index, indexList, std::move(item), std::move(callback));
	}

	void SetUserRate(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		m_databaseUser->Execute({ "Set my rate",
		                          [this, ids = GetSelected(model, index, indexList), item = std::move(item), callback = std::move(callback)]() mutable
		                          {
									  const auto db = m_databaseUser->Database();
									  const auto transaction = db->CreateTransaction();
									  const auto command =
										  transaction->CreateCommand("insert or replace into Books_User(BookID, UserRate, CreatedAt) values(:id, :user_rate, datetime(CURRENT_TIMESTAMP, 'localtime'))");
									  const auto rate = [&]
									  {
										  const auto str = item->GetData(MenuItem::Column::Parameter);
										  assert(!str.isEmpty());
										  bool ok = false;
										  const auto value = str.toInt(&ok);
										  return ok ? value : 0;
									  }();

									  auto ok = std::accumulate(ids.cbegin(),
			                                                    ids.cend(),
			                                                    true,
			                                                    [&](const bool init, const auto id)
			                                                    {
																	command->Bind(":id", id);
																	if (rate)
																		command->Bind(":user_rate", rate);
																	else
																		command->Bind(":user_rate");
																	return command->Execute() && init;
																});
									  ok = transaction->Commit() && ok;

									  return [this, item = std::move(item), callback = std::move(callback), ok](size_t)
									  {
										  if (!ok)
											  m_uiFactory->ShowError(Tr(CANNOT_SET_USER_RATE));

										  callback(item);
										  if (ok)
											  m_dataProvider->RequestBooks(true);
									  };
								  } });
	}

	void SendAsArchive(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		SendAsImpl(model, index, indexList, std::move(item), std::move(callback), &BooksExtractor::ExtractAsArchives);
	}

	void SendAsIs(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		SendAsImpl(model, index, indexList, std::move(item), std::move(callback), &BooksExtractor::ExtractAsIs);
	}

	void SendUnpack(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		SendAsImpl(model, index, indexList, std::move(item), std::move(callback), &BooksExtractor::ExtractUnpack);
	}

	void SendAsInpxCollection(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		SendAsInpxImpl(model,
		               index,
		               indexList,
		               std::move(item),
		               std::move(callback),
		               &IInpxGenerator::ExtractAsInpxCollection,
		               [this] { return m_uiFactory->GetExistingDirectory(Constant::Settings::EXPORT_DIALOG_KEY, SELECT_SEND_TO_FOLDER); });
	}

	void SendAsInpxFile(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		SendAsInpxImpl(model,
		               index,
		               indexList,
		               std::move(item),
		               std::move(callback),
		               &IInpxGenerator::GenerateInpx,
		               [this] { return m_uiFactory->GetSaveFileName(Constant::Settings::EXPORT_DIALOG_KEY, Loc::Tr(Loc::EXPORT, Loc::SELECT_INPX_FILE), Loc::Tr(Loc::EXPORT, Loc::SELECT_INPX_FILE_FILTER)); });
	}

	void SendAsScript(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		const auto commands = m_scriptController->GetCommands(item->GetData(MenuItem::Column::Parameter));
		const auto hasUserDestinationFolder = std::ranges::any_of(commands, [](const auto& command) { return IScriptController::HasMacro(command.args, IScriptController::Macro::UserDestinationFolder); });
		Send(model,
		     index,
		     indexList,
		     std::move(item),
		     std::move(callback),
		     &BooksExtractor::ExtractAsScript,
		     QString("%1/%2").arg(IScriptController::GetMacro(IScriptController::Macro::UserDestinationFolder)).arg(IScriptController::GetMacro(IScriptController::Macro::FileName)),
		     hasUserDestinationFolder);
	}

	void ChangeLanguage(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		auto idList = ILogicFactory::Lock(m_logicFactory)->GetSelectedBookIds(model, index, indexList, { Role::Id });
		if (idList.empty())
			return callback(item);

		if (m_uiFactory->ShowQuestion(Tr(CHANGE_LANGUAGE_CONFIRM).arg(item->GetData(MenuItem::Column::Title)), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return callback(item);

		m_databaseUser->Execute({ "Change book language",
		                          [this, ids = GetSelected(model, index, indexList), item = std::move(item), callback = std::move(callback)]() mutable
		                          {
									  const auto lang = item->GetData(MenuItem::Column::Parameter).toStdString();

									  const auto db = m_databaseUser->Database();
									  const auto transaction = db->CreateTransaction();

									  const auto execute = [&](const char* commandText) -> bool
									  {
										  const auto command = transaction->CreateCommand(commandText);

										  return std::accumulate(ids.cbegin(),
				                                                 ids.cend(),
				                                                 true,
				                                                 [&](const bool init, const auto id)
				                                                 {
																	 command->Bind(":id", id);
																	 command->Bind(":lang", lang);
																	 return command->Execute() && init;
																 });
									  };
									  const auto ok = execute("insert or replace into Books_User(BookID, Lang, CreatedAt) values(:id, :lang, datetime(CURRENT_TIMESTAMP, 'localtime'))")
			                                       && execute("update Books set Lang = :lang where BookID = :id") && transaction->Commit();

									  return [this, item = std::move(item), callback = std::move(callback), ok](size_t)
									  {
										  if (!ok)
											  m_uiFactory->ShowError(Tr(CANNOT_SET_LANGUAGE));

										  callback(item);
										  if (ok)
											  m_dataProvider->RequestBooks(true);
									  };
								  } });
	}

private:
	void SendAsInpxImpl(QAbstractItemModel* model,
	                    const QModelIndex& index,
	                    const QList<QModelIndex>& indexList,
	                    IDataItem::Ptr item,
	                    Callback callback,
	                    void (IInpxGenerator::*extractorMethod)(QString, const std::vector<QString>&, const IBookInfoProvider&, IInpxGenerator::Callback),
	                    const std::function<QString()>& nameGenerator) const
	{
		auto idList = ILogicFactory::Lock(m_logicFactory)->GetSelectedBookIds(model, index, indexList, { Role::Id });
		if (idList.empty())
			return;

		std::transform(std::next(idList.begin()), idList.end(), std::back_inserter(idList.front()), [](auto& id) { return std::move(id.front()); });
		auto inpxName = nameGenerator();
		if (inpxName.isEmpty())
			return callback(item);

		auto extractor = ILogicFactory::Lock(m_logicFactory)->CreateInpxGenerator();
		std::invoke(extractorMethod,
		            *extractor,
		            std::move(inpxName),
		            idList.front(),
		            *m_dataProvider,
		            [extractor, item = std::move(item), callback = std::move(callback)](const bool hasError) mutable
		            {
						item->SetData(QString::number(hasError), MenuItem::Column::HasError);
						callback(item);
						extractor.reset();
					});
	}

	void SendAsImpl(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback, const BooksExtractor::Extract f) const
	{
		auto outputFileNameTemplate = m_settings->Get(Constant::Settings::EXPORT_TEMPLATE_KEY, IScriptController::GetDefaultOutputFileNameTemplate());
		const bool dstFolderRequired = IScriptController::HasMacro(outputFileNameTemplate, IScriptController::Macro::UserDestinationFolder);
		Send(model, index, indexList, std::move(item), std::move(callback), f, std::move(outputFileNameTemplate), dstFolderRequired);
	}

	void Send(QAbstractItemModel* model,
	          const QModelIndex& index,
	          const QList<QModelIndex>& indexList,
	          IDataItem::Ptr item,
	          Callback callback,
	          const BooksExtractor::Extract f,
	          QString outputFileNameTemplate,
	          const bool dstFolderRequired) const
	{
		auto dir = dstFolderRequired ? m_uiFactory->GetExistingDirectory(Constant::Settings::EXPORT_DIALOG_KEY, SELECT_SEND_TO_FOLDER) : QString();
		if (dstFolderRequired && dir.isEmpty())
			return callback(item);

		const auto logicFactory = ILogicFactory::Lock(m_logicFactory);
		auto books = logicFactory->GetExtractedBooks(model, index, indexList);
		auto extractor = logicFactory->CreateBooksExtractor();
		const auto parameter = item->GetData(MenuItem::Column::Parameter);
		((*extractor).*f)(std::move(dir),
		                  parameter,
		                  std::move(books),
		                  std::move(outputFileNameTemplate),
		                  [extractor, item = std::move(item), callback = std::move(callback)](const bool hasError) mutable
		                  {
							  item->SetData(QString::number(hasError), MenuItem::Column::HasError);
							  callback(item);
							  extractor.reset();
						  });
	}

	void GroupAction(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback, const GroupActionFunction f) const
	{
		const auto id = item->GetData(MenuItem::Column::Parameter).toLongLong();
		((*m_groupController).*f)(id, GetSelected(model, index, indexList), [item = std::move(item), callback = std::move(callback)](long long) { callback(item); });
	}

	GroupController::Ids GetSelected(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList) const
	{
		auto selected = ILogicFactory::Lock(m_logicFactory)->GetSelectedBookIds(model, index, indexList, { Role::Id });

		GroupController::Ids ids;
		ids.reserve(selected.size());
		std::ranges::transform(selected, std::inserter(ids, ids.end()), [](const auto& item) { return item.front().toLongLong(); });
		return ids;
	}

	void ChangeBookRemoved(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback, const bool remove) const
	{
		m_databaseUser->Execute({ "Remove books",
		                          [this, ids = GetSelected(model, index, indexList), item = std::move(item), callback = std::move(callback), remove]() mutable
		                          {
									  const bool ok = DatabaseUtil::ChangeBookRemoved(*m_databaseUser->Database(), ids, remove);
									  return [this, item = std::move(item), callback = std::move(callback), remove, ok](size_t)
									  {
										  if (!ok)
											  m_uiFactory->ShowError(Tr(CANNOT_REMOVE_BOOK).arg(Tr(remove ? REMOVE : RESTORE)));

										  callback(item);
										  if (ok)
											  m_dataProvider->RequestBooks(true);
									  };
								  } });
	}

private:
	std::weak_ptr<const ILogicFactory> m_logicFactory;
	std::shared_ptr<const ISettings> m_settings;
	std::shared_ptr<const IReaderController> m_readerController;
	std::shared_ptr<const IDatabaseUser> m_databaseUser;
	std::shared_ptr<const IBookInfoProvider> m_dataProvider;
	std::shared_ptr<const IUiFactory> m_uiFactory;
	PropagateConstPtr<GroupController, std::shared_ptr> m_groupController;
	std::shared_ptr<IScriptController> m_scriptController;
	const int m_starSymbol { m_settings->Get(Constant::Settings::LIBRATE_STAR_SYMBOL_KEY, Constant::Settings::LIBRATE_STAR_SYMBOL_DEFAULT) };
};

void BooksContextMenuProvider::AddTreeMenuItems(const IDataItem::Ptr& parent, const ITreeViewController::RequestContextMenuOptions options)
{
	if (!!(options & ITreeViewController::RequestContextMenuOptions::NodeCollapsed))
		Add(parent, Tr(TREE_EXPAND), BooksMenuAction::Expand);
	if (!!(options & ITreeViewController::RequestContextMenuOptions::NodeExpanded))
		Add(parent, Tr(TREE_COLLAPSE), BooksMenuAction::Collapse);
	if (const auto item = Add(parent, Tr(TREE_COLLAPSE_ALL), BooksMenuAction::CollapseAll); !(options & ITreeViewController::RequestContextMenuOptions::HasExpanded)) //-V821
		item->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);
	if (const auto item = Add(parent, Tr(TREE_EXPAND_ALL), BooksMenuAction::ExpandAll); !(options & ITreeViewController::RequestContextMenuOptions::HasCollapsed)) //-V821
		item->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);
}

BooksContextMenuProvider::BooksContextMenuProvider(const std::shared_ptr<const ILogicFactory>& logicFactory,
                                                   std::shared_ptr<const ISettings> settings,
                                                   std::shared_ptr<const IReaderController> readerController,
                                                   std::shared_ptr<const IDatabaseUser> databaseUser,
                                                   std::shared_ptr<const IBookInfoProvider> dataProvider,
                                                   std::shared_ptr<const IUiFactory> uiFactory,
                                                   std::shared_ptr<GroupController> groupController,
                                                   std::shared_ptr<IScriptController> scriptController)
	: m_impl(logicFactory, std::move(settings), std::move(readerController), std::move(databaseUser), std::move(dataProvider), std::move(uiFactory), std::move(groupController), std::move(scriptController))
{
	PLOGV << "BooksContextMenuProvider created";
}

BooksContextMenuProvider::~BooksContextMenuProvider()
{
	PLOGV << "BooksContextMenuProvider destroyed";
}

void BooksContextMenuProvider::Request(const QModelIndex& index, const ITreeViewController::RequestContextMenuOptions options, Callback callback)
{
	m_impl->Request(index, options, std::move(callback));
}

void BooksContextMenuProvider::OnContextMenuTriggered(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const
{
	const auto invoker = FindSecond(MENU_HANDLERS, static_cast<BooksMenuAction>(item->GetData(MenuItem::Column::Id).toInt()));
	std::invoke(invoker, *m_impl, model, std::cref(index), std::cref(indexList), std::move(item), std::move(callback));
}
