#include "SearchController.h"

#include <QComboBox>
#include <plog/Log.h>

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"
#include "database/interface/IQuery.h"
#include "interface/constants/Enums.h"

#include "interface/constants/Localization.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/INavigationQueryExecutor.h"
#include "interface/ui/IUiFactory.h"
#include "interface/ui/dialogs/IComboBoxTextDialog.h"

#include "util/ISettings.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT               =                   "SearchController";
constexpr auto INPUT_NEW_SEARCH      = QT_TRANSLATE_NOOP("SearchController", "Find books whose titles");
constexpr auto REMOVE_SEARCH_CONFIRM = QT_TRANSLATE_NOOP("SearchController", "Are you sure you want to delete the search results (%1)?");
constexpr auto CANNOT_CREATE_SEARCH  = QT_TRANSLATE_NOOP("SearchController", "Cannot create search query (%1)");
constexpr auto CANNOT_REMOVE_SEARCH  = QT_TRANSLATE_NOOP("SearchController", "Cannot remove search query");
constexpr auto TOO_SHORT_SEARCH      = QT_TRANSLATE_NOOP("SearchController", "Search query is too short. At least %1 characters required.\nTry again?");
constexpr auto SEARCH_TOO_LONG       = QT_TRANSLATE_NOOP("SearchController", "Search query too long.\nTry again?");
constexpr auto SEARCH_ALREADY_EXISTS = QT_TRANSLATE_NOOP("SearchController", "Search query \"%1\" already exists.\nTry again?");

constexpr std::pair<int, const char *> CONDITIONS[]
{
	{ SearchController::SearchMode::Contains  , QT_TRANSLATE_NOOP("SearchController", "contain") },
	{ SearchController::SearchMode::StartsWith, QT_TRANSLATE_NOOP("SearchController", "begin with") },
	{ SearchController::SearchMode::EndsWith  , QT_TRANSLATE_NOOP("SearchController", "end with") },
	{ SearchController::SearchMode::Equals    , QT_TRANSLATE_NOOP("SearchController", "is equal to") },
};

constexpr auto REMOVE_SEARCH_QUERY = "delete from Searches_User where SearchId = ?";
constexpr auto INSERT_SEARCH_QUERY = "insert into Searches_User(Title, Mode, SearchTitle, CreatedAt) values(?, ?, ?, datetime(CURRENT_TIMESTAMP, 'localtime'))";

constexpr auto MINIMUM_SEARCH_LENGTH = 3;

using Names = std::unordered_set<QString>;

QString GetSearchTitle(const QString & value, int mode)
{
	mode = ~mode;
	return QString("%1%2%3").arg(mode & SearchController::SearchMode::StartsWith ? "~" : "", value, mode & SearchController::SearchMode::EndsWith ? "~" : "");
}

long long CreateNewSearchImpl(DB::ITransaction & transaction, const QString & name, const int mode)
{
	assert(!name.isEmpty());
	const auto command = transaction.CreateCommand(INSERT_SEARCH_QUERY);
	command->Bind(0, GetSearchTitle(name, mode).toStdString());
	command->Bind(1, mode);
	command->Bind(2, name.toUpper().toStdString());
	if (!command->Execute())
		return 0;

	const auto query = transaction.CreateQuery(IDatabaseUser::SELECT_LAST_ID_QUERY);
	query->Execute();
	return query->Get<long long>(0);
}

TR_DEF

}

struct SearchController::Impl
{
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	std::shared_ptr<const IDatabaseUser> databaseUser;
	PropagateConstPtr<INavigationQueryExecutor, std::shared_ptr> navigationQueryExecutor;
	std::shared_ptr<const IUiFactory> uiFactory;
	const QString currentCollectionId;

	explicit Impl(std::shared_ptr<ISettings> settings
		, std::shared_ptr<const IDatabaseUser> databaseUser
		, std::shared_ptr<INavigationQueryExecutor> navigationQueryExecutor
		, std::shared_ptr<const IUiFactory> uiFactory
		, const std::shared_ptr<ICollectionController> & collectionController
	)
		: settings(std::move(settings))
		, databaseUser(std::move(databaseUser))
		, navigationQueryExecutor(std::move(navigationQueryExecutor))
		, uiFactory(std::move(uiFactory))
		, currentCollectionId(collectionController->GetActiveCollectionId())
	{
	}

	void GetAllSearches(std::function<void(const Names &)> callback) const
	{
		navigationQueryExecutor->RequestNavigation(NavigationMode::Search, [callback = std::move(callback)] (NavigationMode, const IDataItem::Ptr & root)
		{
			Names names;
			for (size_t i = 0, sz = root->GetChildCount(); i < sz; ++i)
				names.emplace(root->GetChild(i)->GetData().toUpper());
			callback(names);
		});
	}

	void CreateNewSearch(const Names & names, Callback callback)
	{
		auto [searchMode, searchString] = GetNewSearchText(names);
		if (searchString.isEmpty())
			return;

		databaseUser->Execute({ "Create search string", [&, searchMode, searchString = std::move(searchString), callback = std::move(callback)] () mutable
		{
			const auto db = databaseUser->Database();
			const auto transaction = db->CreateTransaction();
			const auto id = CreateNewSearchImpl(*transaction, searchString, searchMode);
			transaction->Commit();

			return [this, id, searchString = std::move(searchString), callback = std::move(callback)] (size_t)
			{
				if (id)
					settings->Set(QString(Constant::Settings::RECENT_NAVIGATION_ID_KEY).arg(currentCollectionId).arg("Search"), QString::number(id));
				else
					uiFactory->ShowError(Tr(CANNOT_CREATE_SEARCH).arg(searchString));
				callback();
			};
		} });
	}

	std::pair<int, QString> GetNewSearchText(const Names & names) const
	{
		while (true)
		{
			const auto searchStringDialog = uiFactory->CreateComboBoxTextDialog(Tr(INPUT_NEW_SEARCH));
			for (const auto & [id, text] : CONDITIONS)
				searchStringDialog->GetComboBox().addItem(Tr(text), id);

			if (searchStringDialog->GetDialog().exec() != QDialog::Accepted)
				return {};

			auto searchString = searchStringDialog->GetLineEdit().text();
			const auto searchMode = searchStringDialog->GetComboBox().currentData().toInt();
			if (searchString.isEmpty())
				return {};

			if (searchString.length() < MINIMUM_SEARCH_LENGTH)
			{
				if (uiFactory->ShowWarning(Tr(TOO_SHORT_SEARCH).arg(MINIMUM_SEARCH_LENGTH), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
					continue;

				return {};
			}

			if (names.contains(GetSearchTitle(searchString, searchMode).toUpper()))
			{
				if (uiFactory->ShowWarning(Tr(SEARCH_ALREADY_EXISTS).arg(searchString), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
					continue;

				return {};
			}

			if (searchString.length() > 148)
			{
				if (uiFactory->ShowWarning(Tr(SEARCH_TOO_LONG), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
					continue;

				return {};
			}

			return std::make_pair(searchMode, std::move(searchString));
		}
	}
};

SearchController::SearchController(std::shared_ptr<ISettings> settings
	, std::shared_ptr<IDatabaseUser> databaseUser
	, std::shared_ptr<INavigationQueryExecutor> navigationQueryExecutor
	, std::shared_ptr<IUiFactory> uiFactory
	, const std::shared_ptr<ICollectionController> & collectionController
)
	: m_impl(std::move(settings)
		, std::move(databaseUser)
		, std::move(navigationQueryExecutor)
		, std::move(uiFactory)
		, collectionController
	)
{
	PLOGV << "SearchController created";
}

SearchController::~SearchController()
{
	PLOGV << "SearchController destroyed";
}

void SearchController::CreateNew(Callback callback)
{
	m_impl->GetAllSearches([&, callback = std::move(callback)] (const Names & names) mutable
	{
		m_impl->CreateNewSearch(names, std::move(callback));
	});
}

void SearchController::Remove(Ids ids, Callback callback) const
{
	if (false
		|| ids.empty()
		|| m_impl->uiFactory->ShowQuestion(Tr(REMOVE_SEARCH_CONFIRM).arg(ids.size()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No
		)
		return;

	m_impl->databaseUser->Execute({ "Remove search string", [&, ids = std::move(ids), callback = std::move(callback)] () mutable
	{
		const auto db = m_impl->databaseUser->Database();
		const auto transaction = db->CreateTransaction();
		const auto command = transaction->CreateCommand(REMOVE_SEARCH_QUERY);

		auto ok = std::accumulate(ids.cbegin(), ids.cend(), true, [&] (const bool init, const Id id)
		{
			command->Bind(0, id);
			return command->Execute() && init;
		});
		ok = transaction->Commit() && ok;

		return [this, callback = std::move(callback), ok] (size_t)
		{
			if (!ok)
				m_impl->uiFactory->ShowError(Tr(CANNOT_REMOVE_SEARCH));

			callback();
		};
	} });
}
