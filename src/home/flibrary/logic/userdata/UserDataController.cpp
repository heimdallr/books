#include "UserDataController.h"

#include <plog/Log.h>

#include "interface/constants/Localization.h"
#include "interface/ui/IUiFactory.h"

#include "shared/DatabaseUser.h"

#include "backup.h"
#include "restore.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "UserData";
constexpr auto SELECT_EXPORT_FILE = QT_TRANSLATE_NOOP("UserData", "Specify a file to export user data");
constexpr auto SELECT_IMPORT_FILE = QT_TRANSLATE_NOOP("UserData", "Select a file to import user data");
constexpr auto FILE_FILTER = QT_TRANSLATE_NOOP("UserData", "Flibrary export files (*.flibk)");
constexpr auto IMPORT_SUCCESS = QT_TRANSLATE_NOOP("UserData", "User data successfully recovered");
constexpr auto EXPORT_SUCCESS = QT_TRANSLATE_NOOP("UserData", "User data successfully saved");
TR_DEF

}

UserDataController::UserDataController(std::shared_ptr<DatabaseUser> databaseUser
	, std::shared_ptr<IUiFactory> uiFactory
)
	: m_databaseUser(std::move(databaseUser))
	, m_uiFactory(std::move(uiFactory))
{
	PLOGD << "UserDataController created";
}

UserDataController::~UserDataController()
{
	PLOGD << "UserDataController created";
}

void UserDataController::Backup(Callback callback) const
{
	Do(std::move(callback), m_uiFactory->GetSaveFileName(Tr(SELECT_EXPORT_FILE), {}, Tr(FILE_FILTER)), EXPORT_SUCCESS, &UserData::Backup);
}

void UserDataController::Restore(Callback callback) const
{
	Do(std::move(callback), m_uiFactory->GetOpenFileName(Tr(SELECT_IMPORT_FILE), {}, Tr(FILE_FILTER)), IMPORT_SUCCESS, &UserData::Restore);
}

void UserDataController::Do(Callback callback, QString fileName, const char * successMessage, const DoFunction f) const
{
	auto executor = m_databaseUser->Executor();
	auto db = m_databaseUser->Database();
	if (fileName.isEmpty())
		return;

	f(*executor, *db, std::move(fileName), [&, successMessage, executor, db, callback = std::move(callback)] (const QString & error) mutable
	{
		error.isEmpty()
			? m_uiFactory->ShowInfo(Tr(successMessage))
			: m_uiFactory->ShowError(Tr(error.toStdString().data()));

		executor.reset();
		db.reset();
		callback();
	});
}
