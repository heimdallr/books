#include "SearchController.h"

#include <plog/Log.h>

#include "database/interface/ITransaction.h"
#include "interface/constants/Enums.h"

#include "interface/constants/Localization.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/INavigationQueryExecutor.h"
#include "interface/ui/IUiFactory.h"

#include "database/DatabaseUser.h"
#include "util/ISettings.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT               =                   "SearchController";
constexpr auto INPUT_NEW_SEARCH      = QT_TRANSLATE_NOOP("SearchController", "Input new search string");
constexpr auto SEARCH                = QT_TRANSLATE_NOOP("SearchController", "Search");
constexpr auto REMOVE_SEARCH_CONFIRM = QT_TRANSLATE_NOOP("SearchController", "Are you sure you want to delete the search results (%1)?");
constexpr auto CANNOT_CREATE_SEARCH  = QT_TRANSLATE_NOOP("SearchController", "Cannot create search query (%1)");
constexpr auto TOO_SHORT_SEARCH      = QT_TRANSLATE_NOOP("SearchController", "Search query is too short. At least %1 characters required.\nTry again?");
constexpr auto SEARCH_TOO_LONG       = QT_TRANSLATE_NOOP("SearchController", "Search query too long.\nTry again?");
constexpr auto SEARCH_ALREADY_EXISTS = QT_TRANSLATE_NOOP("SearchController", "Search query %1 already exists.\nTry again?");

constexpr auto REMOVE_SEARCH_QUERY = "delete from Searches_User where SearchId = ?";
constexpr auto INSERT_SEARCH_QUERY = "insert into Searches_User(Title, CreatedAt) values(?, datetime(CURRENT_TIMESTAMP, 'localtime'))";

constexpr auto MINIMUM_SEARCH_LENGTH = 3;

using Names = std::unordered_set<QString>;

long long CreateNewSearchImpl(DB::ITransaction & transaction, const QString & name)
{
	assert(!name.isEmpty());
	const auto command = transaction.CreateCommand(INSERT_SEARCH_QUERY);
	command->Bind(0, name.toStdString());
	command->Execute();

	const auto query = transaction.CreateQuery(DatabaseUser::SELECT_LAST_ID_QUERY);
	query->Execute();
	return query->Get<long long>(0);
}

TR_DEF

}

struct SearchController::Impl
{
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	std::shared_ptr<const DatabaseUser> databaseUser;
	PropagateConstPtr<INavigationQueryExecutor, std::shared_ptr> navigationQueryExecutor;
	std::shared_ptr<const IUiFactory> uiFactory;
	const QString currentCollectionId;

	explicit Impl(std::shared_ptr<ISettings> settings
		, std::shared_ptr<const DatabaseUser> databaseUser
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
				names.insert(root->GetChild(i)->GetData());
			callback(names);
		});
	}

	void CreateNewSearch(const Names & names, Callback callback)
	{
		auto searchString = GetNewSearchText(names);
		if (searchString.isEmpty())
			return;

		databaseUser->Execute({ "Create search string", [&, searchString = std::move(searchString), callback = std::move(callback)] () mutable
		{
			const auto db = databaseUser->Database();
			const auto transaction = db->CreateTransaction();
			const auto id = CreateNewSearchImpl(*transaction, searchString);
			transaction->Commit();

			return [this, id, searchString = std::move(searchString), callback = std::move(callback)] (size_t)
			{
				if (id)
					settings->Set(QString(Constant::Settings::RECENT_NAVIGATION_ID_KEY).arg(currentCollectionId).arg("Search"), QString::number(id));
				else
					(void)uiFactory->ShowWarning(Tr(CANNOT_CREATE_SEARCH).arg(searchString));
				callback();
			};
		} });
	}

	QString GetNewSearchText(const Names & names) const
	{
		while (true)
		{
			auto searchString = uiFactory->GetText(Tr(INPUT_NEW_SEARCH), Tr(SEARCH));
			if (searchString.isEmpty())
				return {};

			if (searchString.length() < MINIMUM_SEARCH_LENGTH)
			{
				if (uiFactory->ShowWarning(Tr(TOO_SHORT_SEARCH).arg(MINIMUM_SEARCH_LENGTH), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
					continue;

				return {};
			}

			if (names.contains(searchString))
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

			return searchString;
		}
	}
};

SearchController::SearchController(std::shared_ptr<ISettings> settings
	, std::shared_ptr<DatabaseUser> databaseUser
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
	PLOGD << "SearchController created";
}

SearchController::~SearchController()
{
	PLOGD << "SearchController destroyed";
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

		for (const auto & id : ids)
		{
			command->Bind(0, id);
			command->Execute();
		}
		transaction->Commit();

		return [callback = std::move(callback)] (size_t)
		{
			callback();
		};
	} });
}
