#include "UserDataController.h"

#include "interface/localization.h"
#include "interface/constants/ProductConstant.h"

#include "backup.h"
#include "log.h"
#include "restore.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT            = "UserData";
constexpr auto SELECT_EXPORT_FILE = QT_TRANSLATE_NOOP("UserData", "Specify a file to export user data");
constexpr auto SELECT_IMPORT_FILE = QT_TRANSLATE_NOOP("UserData", "Select a file to import user data");
constexpr auto FILE_FILTER        = QT_TRANSLATE_NOOP("UserData", "Flibrary export files (*%1)");
constexpr auto IMPORT_SUCCESS     = QT_TRANSLATE_NOOP("UserData", "User data successfully recovered");
constexpr auto EXPORT_SUCCESS     = QT_TRANSLATE_NOOP("UserData", "User data successfully saved");
constexpr auto DIALOG_KEY         = "Backup";
TR_DEF

}

UserDataController::UserDataController(std::shared_ptr<IDatabaseUser> databaseUser, std::shared_ptr<IUiFactory> uiFactory)
	: m_databaseUser(std::move(databaseUser))
	, m_uiFactory(std::move(uiFactory))
{
	PLOGV << "UserDataController created";
}

UserDataController::~UserDataController()
{
	PLOGV << "UserDataController destroyed";
}

void UserDataController::Backup(Callback callback) const
{
	Backup(m_uiFactory->GetSaveFileName(DIALOG_KEY, Tr(SELECT_EXPORT_FILE), Tr(FILE_FILTER).arg(Constant::BACKUP_FILE_EXT)), std::move(callback));
}

void UserDataController::Restore(Callback callback) const
{
	Restore(m_uiFactory->GetOpenFileName(DIALOG_KEY, Tr(SELECT_IMPORT_FILE), Tr(FILE_FILTER).arg(Constant::BACKUP_FILE_EXT)), std::move(callback));
}

void UserDataController::Backup(QString path, Callback callback) const
{
	Do(std::move(callback), std::move(path), EXPORT_SUCCESS, &UserData::Backup);
}

void UserDataController::Restore(QString path, Callback callback) const
{
	Do(std::move(callback), std::move(path), IMPORT_SUCCESS, &UserData::Restore);
}

void UserDataController::Do(Callback callback, QString fileName, const char* successMessage, const DoFunction f) const
{
	auto executor = m_databaseUser->Executor();
	auto db       = m_databaseUser->Database();
	if (fileName.isEmpty())
		return;

	f(*executor, *db, std::move(fileName), [&, successMessage, executor, db, callback = std::move(callback)](const QString& error) mutable {
		executor.reset();
		db.reset();
		callback(error.isEmpty(), Tr(error.isEmpty() ? successMessage : error.toStdString().data()));
	});
}
