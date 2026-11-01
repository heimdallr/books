#include "BooksContextMenuProvider.h"

#include <ranges>

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"
#include "interface/logic/ICollectionCleaner.h"
#include "interface/logic/IInpxGenerator.h"

#include "ChangeNavigationController/GroupController.h"
#include "database/DatabaseUtil.h"
#include "extract/BooksExtractor.h"
#include "util/language.h"

#include "Constant.h"
#include "MenuItems.h"
#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT                  = "BookContextMenu";
constexpr auto READ_BOOK                = QT_TRANSLATE_NOOP("BookContextMenu", "&Read");
constexpr auto EXPORT                   = QT_TRANSLATE_NOOP("BookContextMenu", "E&xport");
constexpr auto SEND_AS_ARCHIVE          = QT_TRANSLATE_NOOP("BookContextMenu", "As &zip archive");
constexpr auto SEND_AS_IS               = QT_TRANSLATE_NOOP("BookContextMenu", "As &original format");
constexpr auto UNPACK                   = QT_TRANSLATE_NOOP("BookContextMenu", "&Unpack");
constexpr auto SEND_AS_INPX             = QT_TRANSLATE_NOOP("BookContextMenu", "As &inpx collection");
constexpr auto SEND_AS_SINGLE_INPX      = QT_TRANSLATE_NOOP("BookContextMenu", "Generate inde&x file (*.inpx)");
constexpr auto MARK_AS_READ             = QT_TRANSLATE_NOOP("BookContextMenu", "&Mark as read");
constexpr auto SET_MY_RATE              = QT_TRANSLATE_NOOP("BookContextMenu", "&My rate");
constexpr auto REMOVE_MY_RATE           = QT_TRANSLATE_NOOP("BookContextMenu", "&Remove read mark");
constexpr auto CHECK                    = QT_TRANSLATE_NOOP("BookContextMenu", "&Check");
constexpr auto TREE                     = QT_TRANSLATE_NOOP("BookContextMenu", "&Tree");
constexpr auto TREE_COLLAPSE            = QT_TRANSLATE_NOOP("BookContextMenu", "C&ollapse");
constexpr auto TREE_EXPAND              = QT_TRANSLATE_NOOP("BookContextMenu", "E&xpand");
constexpr auto REMOVE_BOOK              = QT_TRANSLATE_NOOP("BookContextMenu", "R&emove");
constexpr auto REMOVE_BOOK_UNDO         = QT_TRANSLATE_NOOP("BookContextMenu", "&Undo deletion");
constexpr auto REMOVE_BOOK_FROM_ARCHIVE = QT_TRANSLATE_NOOP("BookContextMenu", "&Delete permanently");
constexpr auto CHANGE_LANGUAGE          = QT_TRANSLATE_NOOP("BookContextMenu", "Change language");
constexpr auto WITHOUT_RATE             = QT_TRANSLATE_NOOP("BookContextMenu", "No rating");
constexpr auto HASH_SUBMENU             = QT_TRANSLATE_NOOP("BookContextMenu", "Hash");
constexpr auto HASH_CALCULATE           = QT_TRANSLATE_NOOP("BookContextMenu", "Calculate");
constexpr auto HASH_COMPARE             = QT_TRANSLATE_NOOP("BookContextMenu", "Compare");
constexpr auto CLEAR_HISTORY            = QT_TRANSLATE_NOOP("BookContextMenu", "Clear browsing history");

constexpr auto CANNOT_SET_USER_RATE = QT_TRANSLATE_NOOP("BookContextMenu", "Cannot set rate");
constexpr auto CANNOT_SET_LANGUAGE  = QT_TRANSLATE_NOOP("BookContextMenu", "Cannot set language of books");
constexpr auto CANNOT_REMOVE_BOOK   = QT_TRANSLATE_NOOP("BookContextMenu", "Books %1 failed");
constexpr auto REMOVE               = QT_TRANSLATE_NOOP("BookContextMenu", "removing");
constexpr auto RESTORE              = QT_TRANSLATE_NOOP("BookContextMenu", "restoring");

constexpr auto REMOVE_PERMANENTLY_CONFIRM = QT_TRANSLATE_NOOP("BookContextMenu", "The result of this operation cannot be undone. Are you sure you want to delete the books permanently?");
constexpr auto CHANGE_LANGUAGE_CONFIRM    = QT_TRANSLATE_NOOP("BookContextMenu", "Are you sure you want to change the language of the books to %1?");
constexpr auto SAME_NAMED_FILES           = QT_TRANSLATE_NOOP("BookContextMenu", "What to do with the same named files?");
constexpr auto SAME_NAMED_FILES_SKIP      = QT_TRANSLATE_NOOP("BookContextMenu", "Skip");
constexpr auto SAME_NAMED_FILES_OVERWRITE = QT_TRANSLATE_NOOP("BookContextMenu", "Overwrite");
constexpr auto SAME_NAMED_FILES_RENAME    = QT_TRANSLATE_NOOP("BookContextMenu", "Rename");
constexpr auto SAME_NAMED_FILES_CANCEL    = QT_TRANSLATE_NOOP("BookContextMenu", "Cancel");

TR_DEF

constexpr auto USER_RATE_QUERY = "select coalesce(bu.UserRate, -1) from Books b left join Books_User bu on bu.BookID = b.BookID where b.BookID = ?";

struct SendSettings
{
	bool    tempFolder { false };
	bool    createFillTemplateConverterParameter { false };
	QString dstPathSuffix;
};

class IContextMenuHandler // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Function =
		void (IContextMenuHandler::*)(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, BooksContextMenuProvider::Callback callback) const;

public:
	virtual ~IContextMenuHandler() = default;

#define MENU_ACTION_ITEM(NAME) virtual void NAME(QAbstractItemModel *model, const QModelIndex &index, const QList<QModelIndex> &indexList, IDataItem::Ptr item, BooksContextMenuProvider::Callback callback) const = 0;
	GROUPS_MENU_ACTION_ITEMS_X_MACRO
	BOOKS_MENU_ACTION_ITEMS_X_MACRO
#undef MENU_ACTION_ITEM
};

constexpr std::pair<int, IContextMenuHandler::Function> MENU_HANDLERS[] {
#define MENU_ACTION_ITEM(NAME) { BooksMenuAction::NAME, &IContextMenuHandler::NAME },
	BOOKS_MENU_ACTION_ITEMS_X_MACRO
#undef MENU_ACTION_ITEM
#define MENU_ACTION_ITEM(NAME) { GroupsMenuAction::NAME, &IContextMenuHandler::NAME },
		GROUPS_MENU_ACTION_ITEMS_X_MACRO
#undef MENU_ACTION_ITEM
};

void CreateMyRateMenu(const IDataItem::Ptr& root, const QString& id, DB::IDatabase& db, const int starSymbol)
{
	const auto query = db.CreateQuery(USER_RATE_QUERY);
	query->Execute(id.toInt());
	assert(!query->Eof());
	const auto currentUserRate = query->Get<int>(0);

	const auto setEnabled = [](IDataItem& item, const bool enabled) {
		item.SetData(QVariant::fromValue(true).toString(), MenuItem::Column::Checkable)
			.SetData(QVariant::fromValue(enabled).toString(), MenuItem::Column::Enabled)
			.SetData(QVariant::fromValue(!enabled).toString(), MenuItem::Column::Checked);
	};

	const auto parent = AddMenuItem(root, MARK_AS_READ, Tr(MARK_AS_READ));
	setEnabled(AddMenuItem(parent, WITHOUT_RATE, Tr(WITHOUT_RATE), BooksMenuAction::SetUserRate)->SetData(QString::number(0), MenuItem::Column::Parameter), currentUserRate != 0);
	const auto myRate = AddMenuItem(parent, SET_MY_RATE, Tr(SET_MY_RATE));

	for (int rate = 5; rate > 0; --rate)
		setEnabled(
			AddMenuItem(myRate, QString("setRate%1").arg(rate), QString(rate, QChar(starSymbol)), BooksMenuAction::SetUserRate)->SetData(QString::number(rate), MenuItem::Column::Parameter),
			currentUserRate != rate
		);

	AddMenuItem(parent)->SetData(QString::number(-1), MenuItem::Column::Parameter);
	AddMenuItem(parent, REMOVE_MY_RATE, Tr(REMOVE_MY_RATE), BooksMenuAction::SetUserRate)
		->SetData(QString::number(-1), MenuItem::Column::Parameter)
		.SetData(QVariant::fromValue(currentUserRate >= 0).toString(), MenuItem::Column::Enabled);
}

void CreateSendMenu(const IDataItem::Ptr& root, const ITreeViewController::RequestContextMenuOptions options, const IScriptController::Scripts& scripts)
{
	const auto& send = AddMenuItem(root, EXPORT, Tr(EXPORT));
	AddMenuItem(send, SEND_AS_ARCHIVE, Tr(SEND_AS_ARCHIVE), BooksMenuAction::SendAsArchive);
	AddMenuItem(send, SEND_AS_IS, Tr(SEND_AS_IS), BooksMenuAction::SendAsIs);
	if (!!(options & ITreeViewController::RequestContextMenuOptions::IsArchive))
		AddMenuItem(send, UNPACK, Tr(UNPACK), BooksMenuAction::SendUnpack);

	if (!scripts.empty())
	{
		AddMenuItem(send)->SetData(QString::number(-1), MenuItem::Column::Parameter);
		for (const auto& script : scripts)
		{
			auto        title      = script.name;
			const auto& scriptItem = AddMenuItem(send, script.uid, title.replace('&', "&&"), BooksMenuAction::SendAsScript);
			scriptItem->SetData(script.uid, MenuItem::Column::Parameter);
		}
	}
	AddMenuItem(send)->SetData(QString::number(-1), MenuItem::Column::Parameter);
	AddMenuItem(send, SEND_AS_INPX, Tr(SEND_AS_INPX), BooksMenuAction::SendAsInpxCollection);
	AddMenuItem(send, SEND_AS_SINGLE_INPX, Tr(SEND_AS_SINGLE_INPX), BooksMenuAction::SendAsInpxFile);
}

void CreateCheckMenu(const IDataItem::Ptr& root)
{
	const auto parent = AddMenuItem(root, CHECK, Tr(CHECK));
	AddMenuItem(parent, Loc::CHECK_ALL, Loc::Tr(Loc::CONTEXT_MENU, Loc::CHECK_ALL), BooksMenuAction::CheckAll);
	AddMenuItem(parent, Loc::UNCHECK_ALL, Loc::Tr(Loc::CONTEXT_MENU, Loc::UNCHECK_ALL), BooksMenuAction::UncheckAll);
	AddMenuItem(parent, Loc::INVERT_CHECK, Loc::Tr(Loc::CONTEXT_MENU, Loc::INVERT_CHECK), BooksMenuAction::InvertCheck);
}

void CreateChangeLangMenu(const IDataItem::Ptr& root, const QString& currentLocale)
{
	const auto                                         parent = AddMenuItem(root, CHANGE_LANGUAGE, Tr(CHANGE_LANGUAGE));
	std::vector<std::tuple<const char*, QString, int>> languages {
		{ "-1", "", 5000 }
	};
	languages.reserve(std::size(LANGUAGES) + 1);
	std::ranges::transform(LANGUAGES, std::back_inserter(languages), [](const Language& item) {
		return std::make_tuple(item.key, Loc::Tr(LANGUAGES_CONTEXT, item.title), item.priority);
	});
	if (auto it = std::ranges::find(
			languages,
			currentLocale,
			[](const auto& item) {
				return std::get<0>(item);
			}
		);
	    it != languages.end())
	{
		auto& item        = *it;
		std::get<2>(item) = std::numeric_limits<int>::min();
	}
	std::ranges::sort(languages, {}, [](const auto& item) {
		return std::make_tuple(std::get<2>(item), std::get<1>(item));
	});

	for (auto&& [key, value, priority] : languages)
		AddMenuItem(parent, key, std::move(value), priority == 5000 ? INVALID_MENU_ITEM : BooksMenuAction::ChangeLanguage)->SetData(key, MenuItem::Column::Parameter);
}

void CreateHashMenu(const IDataItem::Ptr& root, const ITreeViewController::RequestContextMenuOptions options)
{
	auto submenu = AddMenuItem(root, HASH_SUBMENU, Tr(HASH_SUBMENU));
	AddMenuItem(submenu, HASH_CALCULATE, Tr(HASH_CALCULATE), BooksMenuAction::HashCalculate);
	AddMenuItem(submenu, HASH_COMPARE, Tr(HASH_COMPARE), BooksMenuAction::HashCompare)
		->SetData(QVariant(!!(options & ITreeViewController::RequestContextMenuOptions::HashCompareEnabled)).toString(), MenuItem::Column::Enabled);
}

void CreateTreeMenu(const IDataItem::Ptr& root, const ITreeViewController::RequestContextMenuOptions options)
{
	if (!!(options & ITreeViewController::RequestContextMenuOptions::IsTree))
		BooksContextMenuProvider::AddTreeMenuItems(AddMenuItem(root, TREE, Tr(TREE)), options);
}

} // namespace

class BooksContextMenuProvider::Impl final : public IContextMenuHandler
{
public:
	explicit Impl(
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<const ISettings>            settings,
		std::shared_ptr<const ICollectionProvider>  collectionProvider,
		std::shared_ptr<const IReaderController>    readerController,
		std::shared_ptr<const IDatabaseProvider>    databaseProvider,
		std::shared_ptr<const IBookInfoProvider>    dataProvider,
		std::shared_ptr<const IUiFactory>           uiFactory,
		std::shared_ptr<IScriptController>          scriptController,
		std::shared_ptr<IProgressController>        progressController
	)
		: m_logicFactory { logicFactory }
		, m_settings { std::move(settings) }
		, m_collectionProvider { std::move(collectionProvider) }
		, m_readerController { std::move(readerController) }
		, m_databaseProvider { std::move(databaseProvider) }
		, m_dataProvider { std::move(dataProvider) }
		, m_uiFactory { std::move(uiFactory) }
		, m_scriptController { std::move(scriptController) }
		, m_progressController { std::move(progressController) }
	{
	}

public:
	void Request(const QModelIndex& index, const ITreeViewController::RequestContextMenuOptions options, Callback callback) const
	{
		auto scripts = m_scriptController->GetScripts();
		std::ranges::sort(scripts, [](const auto& lhs, const auto& rhs) {
			return lhs.number < rhs.number;
		});
		auto currentLocale = Loc::GetLocale(*m_settings);

		m_databaseProvider->Execute(
			{ "Create context menu",
			  [id      = index.data(Role::Id).toString(),
			   type    = index.data(Role::Type).value<ItemType>(),
			   removed = index.data(Role::IsRemoved).toBool(),
			   options,
			   starSymbol    = m_starSymbol,
			   callback      = std::move(callback),
			   scripts       = std::move(scripts),
			   currentLocale = std::move(currentLocale)](DB::IDatabase& db) mutable {
				  auto result = MenuItem::Create();

				  if (type == ItemType::Books)
					  AddMenuItem(result, READ_BOOK, Tr(READ_BOOK), BooksMenuAction::ReadBook);

				  CreateSendMenu(result, options, scripts);
				  AddMenuItem(result)->SetData(QString::number(-1), MenuItem::Column::Parameter);

				  if (type == ItemType::Books)
				  {
					  CreateGroupMenu(result, id, db);
					  CreateMyRateMenu(result, id, db, starSymbol);
				  }

				  CreateCheckMenu(result);
				  CreateTreeMenu(result, options);
				  CreateChangeLangMenu(result, currentLocale);
				  CreateHashMenu(result, options);

				  if (!!(options & ITreeViewController::RequestContextMenuOptions::NavigationModeIsHistory))
					  AddMenuItem(result, CLEAR_HISTORY, Tr(CLEAR_HISTORY), BooksMenuAction::ClearHistory);

				  if (type == ItemType::Books)
				  {
					  AddMenuItem(result)->SetData(QString::number(-1), MenuItem::Column::Parameter);
					  AddMenuItem(result, REMOVE_BOOK, Tr(removed ? REMOVE_BOOK_UNDO : REMOVE_BOOK), removed ? BooksMenuAction::UndoRemoveBook : BooksMenuAction::RemoveBook);
					  auto removeItem = AddMenuItem(result, REMOVE_BOOK_FROM_ARCHIVE, Tr(REMOVE_BOOK_FROM_ARCHIVE), BooksMenuAction::RemoveBookFromArchive);
					  if (!(options & ITreeViewController::RequestContextMenuOptions::AllowDestructiveOperations))
						  removeItem->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);
				  }

				  return [callback = std::move(callback), result = std::move(result)](size_t) {
					  if (result->GetChildCount() > 0)
						  callback(result, {});
				  };
			  } }
		);
	}

private: // IContextMenuHandler
	void ReadBook(QAbstractItemModel* /*model*/, const QModelIndex& index, const QList<QModelIndex>& /*indexList*/, IDataItem::Ptr item, Callback callback) const override
	{
		const auto id = index.data(Role::Id).toLongLong();
		m_readerController->Read(id);
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback), id] {
			callback(item, { QString::number(id) });
		});
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

		auto ids = idList | std::views::transform([](const auto& id) {
					   return id.front();
				   })
		         | std::views::common | std::ranges::to<std::set>();

		Util::Remove::Books books;
		books.reserve(idList.size());
		std::ranges::transform(std::move(idList), std::back_inserter(books), [](auto&& idListItem) {
			return Util::Remove::Book { idListItem[0].toLongLong(), std::move(idListItem[1]), std::move(idListItem[2]) };
		});
		auto  cleaner    = ILogicFactory::Lock(m_logicFactory)->CreateCollectionCleaner();
		auto& cleanerRef = *cleaner;
		cleanerRef.RemovePermanently(std::move(books), [&, cleaner = std::move(cleaner), item = std::move(item), callback = std::move(callback), ids = std::move(ids)](const bool result) mutable {
			if (result)
				m_uiFactory->ShowInfo(Loc::Tr(ICollectionCleaner::CONTEXT, ICollectionCleaner::REMOVE_PERMANENTLY_INFO).arg(ids.size()));
			callback(item, ids);
		});
	}

	void UndoRemoveBook(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		ChangeBookRemoved(model, index, indexList, std::move(item), std::move(callback), false);
	}

	void CheckAll(QAbstractItemModel* model, const QModelIndex&, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		model->setData({}, QVariant::fromValue(indexList), Role::CheckAll);
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] {
			callback(item, {});
		});
	}

	void UncheckAll(QAbstractItemModel* model, const QModelIndex&, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		model->setData({}, QVariant::fromValue(indexList), Role::UncheckAll);
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] {
			callback(item, {});
		});
	}

	void InvertCheck(QAbstractItemModel* model, const QModelIndex&, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		model->setData({}, QVariant::fromValue(indexList), Role::InvertCheck);
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] {
			callback(item, {});
		});
	}

	void Collapse(QAbstractItemModel*, const QModelIndex&, const QList<QModelIndex>&, IDataItem::Ptr item, Callback callback) const override
	{
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] {
			callback(item, {});
		});
	}

	void Expand(QAbstractItemModel*, const QModelIndex&, const QList<QModelIndex>&, IDataItem::Ptr item, Callback callback) const override
	{
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] {
			callback(item, {});
		});
	}

	void CollapseAll(QAbstractItemModel*, const QModelIndex&, const QList<QModelIndex>&, IDataItem::Ptr item, Callback callback) const override
	{
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] {
			callback(item, {});
		});
	}

	void ExpandAll(QAbstractItemModel*, const QModelIndex&, const QList<QModelIndex>&, IDataItem::Ptr item, Callback callback) const override
	{
		QTimer::singleShot(0, [item = std::move(item), callback = std::move(callback)] {
			callback(item, {});
		});
	}

	void AddToNewGroup(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		AddToGroup(model, index, indexList, std::move(item), std::move(callback));
	}

	void AddToGroup(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		GroupAction(model, index, indexList, std::move(item), std::move(callback), &GroupController::AddToGroup);
	}

	void MoveToNewGroup(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		MoveToGroup(model, index, indexList, std::move(item), std::move(callback));
	}

	void MoveToGroup(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		GroupAction(model, index, indexList, std::move(item), std::move(callback), &GroupController::MoveToGroup);
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
		m_databaseProvider->ExecuteWritable(
			{ "Set my rate", [this, model = QPointer { model }, ids = GetSelected(model, index, indexList), item = std::move(item), callback = std::move(callback)](DB::IDatabaseWritable& db) mutable {
				 const auto transaction = db.CreateTransaction();
				 const auto command     = transaction->CreateCommand(IDatabaseProvider::SET_USER_RATE_COMMAND);
				 const auto rate        = [&]() -> std::optional<int> {
					 const auto str = item->GetData(MenuItem::Column::Parameter);
					 assert(!str.isEmpty());
					 bool       ok    = false;
					 const auto value = str.toInt(&ok);
					 return ok ? (value >= 0 ? std::optional { value } : std::nullopt) : std::optional { 0 };
				 }();

				 auto ok = std::accumulate(ids.cbegin(), ids.cend(), true, [&](const bool init, const auto id) {
					 return command->Execute(id, rate) && init;
				 });

				 ok = transaction->Commit() && ok;

				 auto idSet = ids | std::views::transform([](const auto& id) {
								  return QString::number(id);
							  })
			                | std::views::common | std::ranges::to<std::set>();

				 return [this, model = std::move(model), item = std::move(item), callback = std::move(callback), ok, rate, ids = std::move(ids), idSet = std::move(idSet)](size_t) mutable {
					 if (!ok)
						 m_uiFactory->ShowError(Tr(CANNOT_SET_USER_RATE));

					 if (ok && !model.isNull())
						 model->setData(
							 {},
							 QVariant::fromValue(SetForIdsArg { .ids = std::move(ids), .column = BookItem::Column::UserRate, .value = rate ? QString::number(*rate) : QString {} }),
							 Role::SetForIds
						 );

					 callback(item, idSet);
				 };
			 } }
		);
	}

	void SendAsArchive(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		SendAsImpl(model, index, indexList, std::move(item), std::move(callback), &BooksExtractor::ExtractAsArchives, { .dstPathSuffix = ".zip" });
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
		SendAsInpxImpl(model, index, indexList, std::move(item), std::move(callback), &IInpxGenerator::ExtractAsInpxCollection);
	}

	void SendAsInpxFile(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		SendAsInpxImpl(model, index, indexList, std::move(item), std::move(callback), &IInpxGenerator::GenerateInpx);
	}

	void SendAsScript(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		const auto commands                 = m_scriptController->GetCommands(item->GetData(MenuItem::Column::Parameter));
		const auto hasUserDestinationFolder = std::ranges::any_of(commands, [](const auto& command) {
			return IScriptController::HasMacro(command.args, IScriptController::Macro::UserDestinationFolder);
		});
		Send(
			model,
			index,
			indexList,
			std::move(item),
			std::move(callback),
			&BooksExtractor::ExtractAsScript,
			QString("%1/%2").arg(IScriptController::GetMacro(IScriptController::Macro::UserDestinationFolder)).arg(IScriptController::GetMacro(IScriptController::Macro::FileName)),
			hasUserDestinationFolder,
			{ .tempFolder = true, .createFillTemplateConverterParameter = true }
		);
	}

	void ChangeLanguage(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		auto idList = ILogicFactory::Lock(m_logicFactory)->GetSelectedBookIds(model, index, indexList, { Role::Id });
		if (idList.empty())
			return callback(item, {});

		if (m_uiFactory->ShowQuestion(Tr(CHANGE_LANGUAGE_CONFIRM).arg(item->GetData(MenuItem::Column::Title)), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return callback(item, {});

		m_databaseProvider->ExecuteWritable(
			{ "Change book language", [this, model = QPointer(model), ids = GetSelected(model, index, indexList), item = std::move(item), callback = std::move(callback)](DB::IDatabaseWritable& db) mutable {
				 auto lang = item->GetData(MenuItem::Column::Parameter).toStdString();

				 const auto transaction = db.CreateTransaction();

				 transaction->CreateCommand("insert or ignore into Languages(LanguageCode) values(?)")->Execute(lang);

				 const auto execute = [&](const char* commandText) -> bool {
					 const auto command = transaction->CreateCommand(commandText);

					 return std::accumulate(ids.cbegin(), ids.cend(), true, [&](const bool init, const auto id) {
						 return command->Execute(lang, id) && init;
					 });
				 };
				 const auto ok = execute(IDatabaseProvider::SET_LANG_COMMAND) && execute("update Books set Lang = ? where BookID = ?") && transaction->Commit();

				 auto idSet = ids | std::views::transform([](const auto& id) {
								  return QString::number(id);
							  })
			                | std::views::common | std::ranges::to<std::set>();

				 return [this, model = std::move(model), item = std::move(item), callback = std::move(callback), lang = std::move(lang), ok, ids = std::move(ids), idSet = std::move(idSet)](size_t) mutable {
					 if (!ok)
						 m_uiFactory->ShowError(Tr(CANNOT_SET_LANGUAGE));

					 if (ok && !model.isNull())
						 model->setData({}, QVariant::fromValue(SetForIdsArg { .ids = std::move(ids), .column = BookItem::Column::Lang, .value = QString::fromStdString(lang) }), Role::SetForIds);

					 callback(item, idSet);
				 };
			 } }
		);
	}

	void HashCalculate(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		auto logicFactory = ILogicFactory::Lock(m_logicFactory);
		auto books        = logicFactory->GetSelectedBookIds(model, index, indexList, { Role::Folder, Role::FileName });

		std::shared_ptr progressItem = m_progressController->Add(std::ssize(books));
		std::shared_ptr executor     = logicFactory->GetExecutor();

		const auto& executorRef = *executor;
		executorRef(
			{ "calculate book hashes",
			  [item         = std::move(item),
			   callback     = std::move(callback),
			   executor     = std::move(executor),
			   folder       = m_collectionProvider->GetActiveCollection().GetFolder(),
			   selectedBook = std::make_pair(index.data(Role::Folder).toString(), index.data(Role::FileName)),
			   books        = std::move(books),
			   progressItem = std::move(progressItem)](auto&) mutable {
				  std::set<QString>  ids;
				  Util::BookHashItem toClipboard;
				  for (auto&& book : books)
				  {
					  ids.emplace(book.front());
					  auto hash = Util::GetHash(folder, std::move(book.front()), book.back());
					  PLOGI << hash;
					  if (hash.folder == selectedBook.first && hash.file == selectedBook.second)
						  toClipboard = std::move(hash);

					  progressItem->Increment(1);
					  if (progressItem->IsStopped())
						  break;
				  }
				  auto serialized = Serialize(toClipboard);
				  return [item = std::move(item), callback = std::move(callback), executor = std::move(executor), serialized = std::move(serialized), ids = std::move(ids)](size_t) {
					  auto* data = new QMimeData;
					  data->setData(Constant::BOOK_HASH_MIME_DATA_TYPE, serialized);
					  QGuiApplication::clipboard()->setMimeData(data);
					  callback(item, ids);
				  };
			  } }
		);
	}

	void HashCompare(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		auto logicFactory = ILogicFactory::Lock(m_logicFactory);
		auto books        = logicFactory->GetSelectedBookIds(model, index, indexList, { Role::Folder, Role::FileName });
		assert(IsOneOf(static_cast<int>(books.size()), 1, 2));

		auto getHash = [](const auto& folder, const auto& book) {
			return Util::GetHash(folder, book.front(), book.back());
		};

		std::shared_ptr executor    = logicFactory->GetExecutor();
		const auto&     executorRef = *executor;
		executorRef(
			{ "compare book hashes",
			  [item         = std::move(item),
			   callback     = std::move(callback),
			   executor     = std::move(executor),
			   folder       = m_collectionProvider->GetActiveCollection().GetFolder(),
			   selectedBook = std::make_pair(index.data(Role::Folder).toString(), index.data(Role::FileName)),
			   books        = std::move(books),
			   getHash      = std::move(getHash)](auto&) mutable {
				  auto lhs = books.size() > 1 ? getHash(folder, books.front()) : [] {
					  const auto data = QGuiApplication::clipboard()->mimeData();
					  assert(data && data->hasFormat(Constant::BOOK_HASH_MIME_DATA_TYPE));
					  const auto bytes = data->data(Constant::BOOK_HASH_MIME_DATA_TYPE);
					  return Util::Deserialize(bytes);
				  }(), rhs = getHash(folder, books.back());

				  const auto updateItem = [](Util::BookHashItem& bookHashItem) {
					  if (bookHashItem.cover.hash.isEmpty())
						  return;

					  bookHashItem.cover.file = Global::COVER;
					  decltype(lhs.images) images;
					  images.reserve(bookHashItem.images.size() + 1);
					  images.emplace_back(std::move(bookHashItem.cover));
					  std::ranges::move(std::move(bookHashItem.images), std::back_inserter(images));
					  bookHashItem.images = std::move(images);
					  bookHashItem.cover  = {};
				  };

				  updateItem(lhs);
				  updateItem(rhs);

				  auto     result = Compare(lhs, rhs).join('\n');
				  std::set ids { books.front().front(), books.front().back() };

				  return [item = std::move(item), callback = std::move(callback), executor = std::move(executor), result = std::move(result), ids = std::move(ids)](size_t) {
					  PLOGI << result;
					  callback(item, ids);
				  };
			  } }
		);
	}

	void ClearHistory(QAbstractItemModel*, const QModelIndex&, const QList<QModelIndex>&, IDataItem::Ptr item, Callback callback) const override
	{
		m_databaseProvider->ExecuteWritable({ "Clear history", [this, item = std::move(item), callback = std::move(callback)](DB::IDatabaseWritable& db) mutable {
												 const auto lang = item->GetData(MenuItem::Column::Parameter).toStdString();
												 const auto tr   = db.CreateTransaction();
												 tr->CreateCommand(std::format("delete from {}", DatabaseUtil::GetHistoryTableName(*m_settings)))->Execute();
												 tr->Commit();
												 return [this, item = std::move(item), callback = std::move(callback)](size_t) {
													 callback(item, {});
													 m_dataProvider->RequestBooks(true);
												 };
											 } });
	}

private:
	void SendAsInpxImpl(
		QAbstractItemModel*       model,
		const QModelIndex&        index,
		const QList<QModelIndex>& indexList,
		IDataItem::Ptr            item,
		Callback                  callback,
		void (IInpxGenerator::*extractorMethod)(QString, const std::vector<QString>&, const IBookInfoProvider&, IInpxGenerator::Callback)
	) const
	{
		auto idList = ILogicFactory::Lock(m_logicFactory)->GetSelectedBookIds(model, index, indexList, { Role::Id });
		if (idList.empty())
			return;

		std::transform(std::next(idList.begin()), idList.end(), std::back_inserter(idList.front()), [](auto& id) {
			return std::move(id.front());
		});

		auto inpxName = m_uiFactory->GetSaveFileName(Constant::Settings::EXPORT_DIALOG_KEY, Loc::Tr(Loc::EXPORT, Loc::SELECT_INPX_FILE), Loc::Tr(Loc::EXPORT, Loc::SELECT_INPX_FILE_FILTER));
		if (inpxName.isEmpty())
			return callback(item, {});

		auto ids = idList | std::views::transform([](const auto& id) {
					   return id.front();
				   })
		         | std::views::common | std::ranges::to<std::set>();

		auto extractor = ILogicFactory::Lock(m_logicFactory)->CreateInpxGenerator();
		std::invoke(
			extractorMethod,
			*extractor,
			std::move(inpxName),
			idList.front(),
			*m_dataProvider,
			[extractor, item = std::move(item), callback = std::move(callback), ids = std::move(ids)](const bool hasError) mutable {
				item->SetData(QString::number(hasError), MenuItem::Column::HasError);
				callback(item, ids);
				extractor.reset();
			}
		);
	}

	void SendAsImpl(
		QAbstractItemModel*           model,
		const QModelIndex&            index,
		const QList<QModelIndex>&     indexList,
		IDataItem::Ptr                item,
		Callback                      callback,
		const BooksExtractor::Extract f,
		const SendSettings&           sendSettings = {}
	) const
	{
		auto       outputFileNameTemplate = m_settings->Get(Constant::Settings::EXPORT_TEMPLATE_KEY, IScriptController::GetDefaultOutputFileNameTemplate());
		const bool dstFolderRequired      = IScriptController::HasMacro(outputFileNameTemplate, IScriptController::Macro::UserDestinationFolder);
		Send(model, index, indexList, std::move(item), std::move(callback), f, outputFileNameTemplate, dstFolderRequired, sendSettings);
	}

	void Send(
		QAbstractItemModel*           model,
		const QModelIndex&            index,
		const QList<QModelIndex>&     indexList,
		IDataItem::Ptr                item,
		Callback                      callback,
		const BooksExtractor::Extract f,
		const QString&                outputFileNameTemplate,
		const bool                    dstFolderRequired,
		const SendSettings&           sendSettings
	) const
	{
		auto dir = dstFolderRequired ? m_uiFactory->GetExistingDirectory(Constant::Settings::EXPORT_DIALOG_KEY, Loc::SELECT_SEND_TO_FOLDER) : QString();
		if (dstFolderRequired && dir.isEmpty())
			return callback(item, {});

		const auto logicFactory = ILogicFactory::Lock(m_logicFactory);

		auto books = logicFactory->GetExtractedBooks(model, index, indexList);

		const auto fillTemplateConverter = logicFactory->CreateFillTemplateConverter(sendSettings.createFillTemplateConverterParameter);

		std::shared_ptr<QTemporaryDir> tempDir;
		if (sendSettings.tempFolder)
			tempDir = std::make_shared<QTemporaryDir>();

		const auto db = m_databaseProvider->GetDatabase();
		for (auto& book : books)
			fillTemplateConverter->Fill(*db, outputFileNameTemplate, book, tempDir ? tempDir->filePath("") : dir);

		if (CheckUniqueFileNames(books, sendSettings.dstPathSuffix))
			return callback(item, {});

		auto       ids       = books | std::views::transform([](const auto book) {
					   return QString::number(book.id);
							   })
		                     | std::ranges::to<std::set<QString>>();
		auto       extractor = logicFactory->CreateBooksExtractor();
		const auto parameter = item->GetData(MenuItem::Column::Parameter);
		((*extractor)
		 .*f)(dir, parameter, std::move(books), [extractor, item = std::move(item), ids = std::move(ids), tempDir = std::move(tempDir), callback = std::move(callback)](const bool hasError) mutable {
			item->SetData(QString::number(hasError), MenuItem::Column::HasError);
			callback(item, ids);
			extractor.reset();
		});
	}

	void GroupAction(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback, const GroupActionFunction f) const
	{
		auto        controller    = ILogicFactory::Lock(m_logicFactory)->CreateGroupController();
		const auto& controllerRef = *controller;
		const auto  ids           = GetSelected(model, index, indexList);
		auto        idSet         = ids | std::views::transform([](const auto& id) {
						 return QString::number(id);
									})
		                          | std::views::common | std::ranges::to<std::set>();
		const auto  id            = item->GetData(MenuItem::Column::Parameter).toLongLong();
		(controllerRef.*f)(id, ids, [item = std::move(item), callback = std::move(callback), controller = std::move(controller), idSet = std::move(idSet)](long long) mutable {
			callback(item, idSet);
			controller.reset();
		});
	}

	GroupController::Ids GetSelected(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList) const
	{
		auto selected = ILogicFactory::Lock(m_logicFactory)->GetSelectedBookIds(model, index, indexList, { Role::Id });

		GroupController::Ids ids;
		ids.reserve(selected.size());
		std::ranges::transform(selected, std::inserter(ids, ids.end()), [](const auto& item) {
			return item.front().toLongLong();
		});
		return ids;
	}

	void ChangeBookRemoved(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback, const bool remove) const
	{
		m_databaseProvider->ExecuteWritable(
			{ "Remove books", [this, ids = GetSelected(model, index, indexList), item = std::move(item), callback = std::move(callback), remove](DB::IDatabaseWritable& db) mutable {
				 const bool ok    = DatabaseUtil::ChangeBookRemoved(db, ids, remove);
				 auto       idSet = ids | std::views::transform([](const auto& id) {
								  return QString::number(id);
									})
			                      | std::views::common | std::ranges::to<std::set>();
				 return [this, item = std::move(item), callback = std::move(callback), remove, ok, idSet = std::move(idSet)](size_t) {
					 if (!ok)
						 m_uiFactory->ShowError(Tr(CANNOT_REMOVE_BOOK).arg(Tr(remove ? REMOVE : RESTORE)));

					 callback(item, idSet);
					 if (ok)
						 m_dataProvider->RequestBooks(true);
				 };
			 } }
		);
	}

	bool CheckUniqueFileNames(Util::ExtractedBooks& books, const QString& dstPathSuffix) const
	{
		const auto whatTodo = [&] {
			std::unordered_set<QString> uniqueNames;
			if (std::ranges::any_of(books, [&](const auto& book) {
					auto checkPath = book.dstFileName + dstPathSuffix;
					return QFile::exists(checkPath) || !uniqueNames.emplace(checkPath).second;
				}))
				return m_uiFactory->ShowCustomDialog(
					QMessageBox::Question,
					Loc::Tr(Loc::Ctx::COMMON, Loc::WARNING),
					Tr(SAME_NAMED_FILES),
					{
						{      QMessageBox::AcceptRole,      Tr(SAME_NAMED_FILES_SKIP) },
						{ QMessageBox::DestructiveRole, Tr(SAME_NAMED_FILES_OVERWRITE) },
						{          QMessageBox::NoRole,    Tr(SAME_NAMED_FILES_RENAME) },
						{      QMessageBox::RejectRole,    Tr(SAME_NAMED_FILES_CANCEL) },
				},
					QMessageBox::AcceptRole
				);

			return QMessageBox::NoRole;
		}();

		switch (whatTodo)
		{
			case QMessageBox::RejectRole:
				return true;
			case QMessageBox::NoRole:
				return false;
			default:
				break;
		}

		const auto filesExist = books | std::views::transform([](const auto& item) {
									return item.dstFileName;
								})
		                      | std::views::filter([&](const auto& item) {
									return QFile::exists(item + dstPathSuffix);
								})
		                      | std::ranges::to<std::unordered_set>();

		switch (whatTodo)
		{
			case QMessageBox::DestructiveRole:
				for (const auto& file : filesExist)
					QFile::remove(file + dstPathSuffix);
				return false;
			case QMessageBox::AcceptRole:
				std::erase_if(books, [&](const auto& item) {
					return filesExist.contains(item.dstFileName);
				});
				return books.empty();
			default:
				break;
		}

		return false;
	}

private:
	std::weak_ptr<const ILogicFactory>                    m_logicFactory;
	std::shared_ptr<const ISettings>                      m_settings;
	std::shared_ptr<const ICollectionProvider>            m_collectionProvider;
	std::shared_ptr<const IReaderController>              m_readerController;
	std::shared_ptr<const IDatabaseProvider>              m_databaseProvider;
	std::shared_ptr<const IBookInfoProvider>              m_dataProvider;
	std::shared_ptr<const IUiFactory>                     m_uiFactory;
	PropagateConstPtr<IScriptController, std::shared_ptr> m_scriptController;
	std::shared_ptr<IProgressController>                  m_progressController;
	const int                                             m_starSymbol { m_settings->Get(Constant::Settings::PREFER_USER_RATE_STAR_SYMBOL_KEY, Constant::Settings::STAR_SYMBOL_DEFAULT) };
};

void BooksContextMenuProvider::AddTreeMenuItems(const IDataItem::Ptr& parent, const ITreeViewController::RequestContextMenuOptions options)
{
	if (!!(options & ITreeViewController::RequestContextMenuOptions::NodeCollapsed))
		AddMenuItem(parent, TREE_EXPAND, Tr(TREE_EXPAND), BooksMenuAction::Expand);
	if (!!(options & ITreeViewController::RequestContextMenuOptions::NodeExpanded))
		AddMenuItem(parent, TREE_COLLAPSE, Tr(TREE_COLLAPSE), BooksMenuAction::Collapse);
	AddMenuItem(parent, Loc::TREE_COLLAPSE_ALL, Loc::Tr(Loc::CONTEXT_MENU, Loc::TREE_COLLAPSE_ALL), BooksMenuAction::CollapseAll)
		->SetData(QVariant(!!(options & ITreeViewController::RequestContextMenuOptions::HasExpanded)).toString(), MenuItem::Column::Enabled);
	AddMenuItem(parent, Loc::TREE_EXPAND_ALL, Loc::Tr(Loc::CONTEXT_MENU, Loc::TREE_EXPAND_ALL), BooksMenuAction::ExpandAll)
		->SetData(QVariant(!!(options & ITreeViewController::RequestContextMenuOptions::HasCollapsed)).toString(), MenuItem::Column::Enabled);
}

BooksContextMenuProvider::BooksContextMenuProvider(
	const std::shared_ptr<const ILogicFactory>& logicFactory,
	std::shared_ptr<const ISettings>            settings,
	std::shared_ptr<const ICollectionProvider>  collectionProvider,
	std::shared_ptr<const IReaderController>    readerController,
	std::shared_ptr<const IDatabaseProvider>    databaseProvider,
	std::shared_ptr<const IBookInfoProvider>    dataProvider,
	std::shared_ptr<const IUiFactory>           uiFactory,
	std::shared_ptr<IScriptController>          scriptController,
	std::shared_ptr<IMainProgressController>    progressController
)
	: m_impl(
		  logicFactory,
		  std::move(settings),
		  std::move(collectionProvider),
		  std::move(readerController),
		  std::move(databaseProvider),
		  std::move(dataProvider),
		  std::move(uiFactory),
		  std::move(scriptController),
		  std::move(progressController)
	  )
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
	const auto invoker = FindSecond(MENU_HANDLERS, item->GetData(MenuItem::Column::Id).toInt());
	std::invoke(invoker, *m_impl, model, std::cref(index), std::cref(indexList), std::move(item), std::move(callback));
}
