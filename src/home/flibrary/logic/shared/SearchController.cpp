#include "SearchController.h"

#include <plog/Log.h>

#include "database/interface/ITransaction.h"

#include "interface/constants/Localization.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/ui/IUiFactory.h"

#include "shared/DatabaseUser.h"
#include "userdata/constants/searches.h"
#include "util/ISettings.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT               =                   "SearchController";
constexpr auto INPUT_NEW_SEARCH      = QT_TRANSLATE_NOOP("SearchController", "Input new search string");
constexpr auto SEARCH                = QT_TRANSLATE_NOOP("SearchController", "Search");
constexpr auto REMOVE_SEARCH_CONFIRM = QT_TRANSLATE_NOOP("SearchController", "Are you sure you want to delete the search results (%1)?");
constexpr auto CANNOT_CREATE_SEARCH  = QT_TRANSLATE_NOOP("SearchController", "Cannot create search query (%1)");
constexpr auto TOO_SHORT_SEARCH      = QT_TRANSLATE_NOOP("SearchController", "Search query is too short. At least %1 characters required");

constexpr auto REMOVE_SEARCH_QUERY = "delete from Searches_User where SearchId = ?";

constexpr auto MINIMUM_SEARCH_LENGTH = 3;

long long CreateNewSearchImpl(DB::ITransaction & transaction, const QString & name)
{
	assert(!name.isEmpty());
	const auto command = transaction.CreateCommand(Constant::UserData::Searches::CreateNewSearchCommandText);
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
	PropagateConstPtr<DatabaseUser, std::shared_ptr> databaseUser;
	PropagateConstPtr<IUiFactory, std::shared_ptr> uiFactory;

	explicit Impl(std::shared_ptr<ISettings> settings
		, std::shared_ptr<DatabaseUser> databaseUser
		, std::shared_ptr<IUiFactory> uiFactory
	)
		: settings(std::move(settings))
		, databaseUser(std::move(databaseUser))
		, uiFactory(std::move(uiFactory))
	{
	}

	void CreateNewSearch(Callback callback)
	{
		auto searchString = uiFactory->GetText(Tr(INPUT_NEW_SEARCH), Tr(SEARCH));
		if (searchString.isEmpty())
			return;

		if (searchString.length() < MINIMUM_SEARCH_LENGTH)
			return (void)uiFactory->ShowWarning(Tr(TOO_SHORT_SEARCH).arg(MINIMUM_SEARCH_LENGTH));

		databaseUser->Execute({ "Create search string", [&, searchString = std::move(searchString), callback = std::move(callback)] () mutable
		{
			const auto db = databaseUser->Database();
			const auto transaction = db->CreateTransaction();
			const auto id = CreateNewSearchImpl(*transaction, searchString);
			transaction->Commit();

			return [&, id, searchString = std::move(searchString), callback = std::move(callback)] (size_t)
			{
				if (id)
					settings->Set(QString(Constant::Settings::RECENT_NAVIGATION_ID_KEY).arg("Search"), QString::number(id));
				else
					uiFactory->ShowWarning(Tr(CANNOT_CREATE_SEARCH).arg(searchString));
				callback();
			};
		} });
	}
};

SearchController::SearchController(std::shared_ptr<ISettings> settings
	, std::shared_ptr<DatabaseUser> databaseUser
	, std::shared_ptr<IUiFactory> uiFactory
)
	: m_impl(std::move(settings), std::move(databaseUser), std::move(uiFactory))
{
	PLOGD << "SearchController created";
}

SearchController::~SearchController()
{
	PLOGD << "SearchController destroyed";
}

void SearchController::CreateNew(Callback callback)
{
	m_impl->CreateNewSearch(std::move(callback));
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