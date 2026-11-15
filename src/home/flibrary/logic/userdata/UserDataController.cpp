#include "UserDataController.h"

#include "interface/constants/ProductConstant.h"
#include "interface/localization.h"

#include "constants/UserData.h"

#include "backup.h"
#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT            = "UserData";
constexpr auto SELECT_EXPORT_FILE = QT_TRANSLATE_NOOP("UserData", "Specify a file to export user data");
constexpr auto SELECT_IMPORT_FILE = QT_TRANSLATE_NOOP("UserData", "Select a file to import user data");
constexpr auto EXPORT_FILE_FILTER = QT_TRANSLATE_NOOP("UserData", "FLibrary user data files (*%1);;All files (*.*)");
constexpr auto IMPORT_FILE_FILTER = QT_TRANSLATE_NOOP("UserData", "FLibrary user data files (*%1);;MyHomeLib user data files (*.mhlud2);;All files (*.*)");
constexpr auto IMPORT_SUCCESS     = QT_TRANSLATE_NOOP("UserData", "User data successfully recovered");
constexpr auto EXPORT_SUCCESS     = QT_TRANSLATE_NOOP("UserData", "User data successfully saved");
constexpr auto CANNOT_WRITE       = QT_TRANSLATE_NOOP("UserData", "Cannot write to '%1'");
constexpr auto DIALOG_KEY         = "Backup";
TR_DEF

} // namespace

namespace HomeCompa::Flibrary::UserData {

void Restore(const IDatabaseProvider& databaseProvider, QIODevice& stream, Callback callback);
void ImportFromMyHomeLib(const IDatabaseProvider& databaseProvider, QIODevice& stream, Callback callback);

} // namespace HomeCompa::Flibrary::UserData

UserDataController::UserDataController(std::shared_ptr<const IDatabaseProvider> databaseProvider, std::shared_ptr<IUiFactory> uiFactory)
	: m_databaseProvider { std::move(databaseProvider) }
	, m_uiFactory { std::move(uiFactory) }
{
	PLOGV << "UserDataController created";
}

UserDataController::~UserDataController()
{
	PLOGV << "UserDataController destroyed";
}

void UserDataController::Backup(Callback callback) const
{
	Backup(m_uiFactory->GetSaveFileName(DIALOG_KEY, Tr(SELECT_EXPORT_FILE), Tr(EXPORT_FILE_FILTER).arg(Constant::BACKUP_FILE_EXT)), std::move(callback));
}

void UserDataController::Restore(Callback callback) const
{
	Restore(m_uiFactory->GetOpenFileName(DIALOG_KEY, Tr(SELECT_IMPORT_FILE), Tr(IMPORT_FILE_FILTER).arg(Constant::BACKUP_FILE_EXT)), std::move(callback));
}

void UserDataController::Backup(const QString& path, Callback callback) const
{
	if (path.isEmpty())
		return callback(false, QString {});

	QFile out(path);
	if (out.open(QIODevice::WriteOnly))
		return Backup(out, std::move(callback));

	const auto error = QString(CANNOT_WRITE).arg(path);
	PLOGE << error;
	return callback(false, error);
}

void UserDataController::Backup(QIODevice& stream, Callback callback) const
{
	Do(std::move(callback), stream, EXPORT_SUCCESS, &UserData::Backup);
}

void UserDataController::Restore(QIODevice& stream, const RestoreMode mode, Callback callback) const
{
	const auto f = mode == RestoreMode::MyHomeLib ? &UserData::ImportFromMyHomeLib : &UserData::Restore;
	Do(std::move(callback), stream, IMPORT_SUCCESS, f);
}

void UserDataController::Restore(const QString& path, Callback callback) const
{
	if (path.isEmpty())
		return callback(false, QString {});

	QFile inp(path);
	if (!inp.open(QIODevice::ReadOnly))
		return callback(false, QString(UserData::CANNOT_READ_FROM).arg(path));

	const auto mode = path.endsWith(".mhlud2", Qt::CaseInsensitive) ? RestoreMode::MyHomeLib : RestoreMode::FLibrary;
	Restore(inp, mode, std::move(callback));
}

void UserDataController::Do(Callback callback, QIODevice& stream, const char* successMessage, const DoFunction f) const
{
	f(*m_databaseProvider, std::ref(stream), [&, successMessage, callback = std::move(callback)](const QString& error) mutable {
		callback(error.isEmpty(), Tr(error.isEmpty() ? successMessage : error.toStdString().data()));
	});
}
