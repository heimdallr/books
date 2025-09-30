#include "SearchController.h"

#include <QComboBox>

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT               = "SearchController";
constexpr auto INPUT_NEW_SEARCH      = QT_TRANSLATE_NOOP("SearchController", "Search books");
constexpr auto SEARCH_QUERY          = QT_TRANSLATE_NOOP("SearchController", "Search query");
constexpr auto REMOVE_SEARCH_CONFIRM = QT_TRANSLATE_NOOP("SearchController", "Are you sure you want to delete the search results (%1)?");
constexpr auto CANNOT_CREATE_SEARCH  = QT_TRANSLATE_NOOP("SearchController", "Cannot create search query (%1)");
constexpr auto CANNOT_REMOVE_SEARCH  = QT_TRANSLATE_NOOP("SearchController", "Cannot remove search query");
constexpr auto TOO_SHORT_SEARCH      = QT_TRANSLATE_NOOP("SearchController", "Search query is too short. At least %1 characters required.\nTry again?");
constexpr auto SEARCH_TOO_LONG       = QT_TRANSLATE_NOOP("SearchController", "Search query too long.\nTry again?");
constexpr auto SEARCH_ALREADY_EXISTS = QT_TRANSLATE_NOOP("SearchController", "Search query \"%1\" already exists.\nTry again?");

constexpr auto REMOVE_SEARCH_QUERY = "delete from Searches_User where SearchId = ?";
constexpr auto INSERT_SEARCH_QUERY = "insert into Searches_User(Title, CreatedAt) values(?, datetime(CURRENT_TIMESTAMP, 'localtime'))";

constexpr auto MINIMUM_SEARCH_LENGTH = 3;

using Names = std::unordered_map<QString, long long>;

QString GetSearchString(const QString& str)
{
	auto splitted = str.split(' ', Qt::SkipEmptyParts);
	std::ranges::transform(splitted, splitted.begin(), [](const QString& item) {
		return item + "*";
	});
	return splitted.join(' ');
}

long long CreateNewSearchImpl(DB::ITransaction& transaction, const QString& name)
{
	assert(!name.isEmpty());
	const auto command = transaction.CreateCommand(INSERT_SEARCH_QUERY);
	command->Bind(0, GetSearchString(name).toStdString());
	if (!command->Execute())
		return 0;

	const auto query = transaction.CreateQuery(IDatabaseUser::SELECT_LAST_ID_QUERY);
	query->Execute();
	return query->Get<long long>(0);
}

TR_DEF

} // namespace

struct SearchController::Impl
{
	std::shared_ptr<const IDatabaseUser>                         databaseUser;
	PropagateConstPtr<INavigationQueryExecutor, std::shared_ptr> navigationQueryExecutor;
	std::shared_ptr<const IUiFactory>                            uiFactory;
	const QString                                                currentCollectionId;

	explicit Impl(
		const ICollectionController&              collectionController,
		std::shared_ptr<const IDatabaseUser>      databaseUser,
		std::shared_ptr<INavigationQueryExecutor> navigationQueryExecutor,
		std::shared_ptr<const IUiFactory>         uiFactory
	)
		: databaseUser { std::move(databaseUser) }
		, navigationQueryExecutor { std::move(navigationQueryExecutor) }
		, uiFactory { std::move(uiFactory) }
		, currentCollectionId { collectionController.GetActiveCollectionId() }
	{
	}

	void GetAllSearches(std::function<void(const Names&)> callback) const
	{
		navigationQueryExecutor->RequestNavigation(NavigationMode::Search, [callback = std::move(callback)](NavigationMode, const IDataItem::Ptr& root) {
			Names names;
			for (size_t i = 0, sz = root->GetChildCount(); i < sz; ++i)
			{
				const auto  childPtr = root->GetChild(i);
				const auto& child    = *childPtr;
				names.try_emplace(child.GetData().toUpper(), child.GetId().toLongLong());
			}
			callback(names);
		});
	}

	void CreateNewSearch(const Names& names, Callback callback)
	{
		auto searchString = GetNewSearchText(names);
		if (searchString.isEmpty())
			return callback(-1);

		CreateNewSearch(std::move(callback), std::move(searchString));
	}

	void CreateNewSearch(Callback callback, QString searchString)
	{
		databaseUser->Execute({ "Create search string", [&, searchString = std::move(searchString), callback = std::move(callback)]() mutable {
								   const auto db          = databaseUser->Database();
								   const auto transaction = db->CreateTransaction();
								   const auto id          = CreateNewSearchImpl(*transaction, searchString);
								   transaction->Commit();

								   return [this, id, searchString = std::move(searchString), callback = std::move(callback)](size_t) {
									   if (!id)
										   uiFactory->ShowError(Tr(CANNOT_CREATE_SEARCH).arg(searchString));
									   callback(id);
								   };
							   } });
	}

	void FindOrCreateNewSearch(const Names& names, QString searchString, Callback callback)
	{
		if (searchString.length() < MINIMUM_SEARCH_LENGTH)
			return callback(-1);

		if (const auto it = names.find(GetSearchString(searchString).toUpper()); it != names.end())
			return callback(it->second);

		CreateNewSearch(std::move(callback), std::move(searchString));
	}

	QString GetNewSearchText(const Names& names) const
	{
		while (true)
		{
			auto searchString = uiFactory->GetText(Tr(INPUT_NEW_SEARCH), Tr(SEARCH_QUERY));
			if (searchString.isEmpty())
				return {};

			if (searchString.length() < MINIMUM_SEARCH_LENGTH)
			{
				if (uiFactory->ShowWarning(Tr(TOO_SHORT_SEARCH).arg(MINIMUM_SEARCH_LENGTH), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
					continue;

				return {};
			}

			if (names.contains(searchString.toUpper()))
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

SearchController::SearchController(
	const std::shared_ptr<const ICollectionController>& collectionController,
	std::shared_ptr<IDatabaseUser>                      databaseUser,
	std::shared_ptr<INavigationQueryExecutor>           navigationQueryExecutor,
	std::shared_ptr<IUiFactory>                         uiFactory
)
	: m_impl(*collectionController, std::move(databaseUser), std::move(navigationQueryExecutor), std::move(uiFactory))
{
	PLOGV << "SearchController created";
}

SearchController::~SearchController()
{
	PLOGV << "SearchController destroyed";
}

void SearchController::CreateNew(Callback callback)
{
	m_impl->GetAllSearches([&, callback = std::move(callback)](const Names& names) mutable {
		m_impl->CreateNewSearch(names, std::move(callback));
	});
}

void SearchController::Remove(Ids ids, Callback callback) const
{
	if (ids.empty() || m_impl->uiFactory->ShowQuestion(Tr(REMOVE_SEARCH_CONFIRM).arg(ids.size()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
		return;

	m_impl->databaseUser->Execute({ "Remove search string", [&, ids = std::move(ids), callback = std::move(callback)]() mutable {
									   const auto db          = m_impl->databaseUser->Database();
									   const auto transaction = db->CreateTransaction();
									   const auto command     = transaction->CreateCommand(REMOVE_SEARCH_QUERY);

									   auto ok = std::accumulate(ids.cbegin(), ids.cend(), true, [&](const bool init, const Id id) {
										   command->Bind(0, id);
										   return command->Execute() && init;
									   });
									   ok      = transaction->Commit() && ok;

									   return [this, callback = std::move(callback), ok](size_t) {
										   if (!ok)
											   m_impl->uiFactory->ShowError(Tr(CANNOT_REMOVE_SEARCH));

										   callback(-1);
									   };
								   } });
}

void SearchController::Search(QString searchString, Callback callback)
{
	m_impl->GetAllSearches([&, searchString = std::move(searchString), callback = std::move(callback)](const Names& names) mutable {
		m_impl->FindOrCreateNewSearch(names, std::move(searchString), std::move(callback));
	});
}
