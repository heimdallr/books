#pragma once

namespace HomeCompa::Flibrary::Constant
{

constexpr auto UI = "ui";

constexpr auto ITEM = "Item";
constexpr auto TITLE = "Title";
constexpr auto VALUE = "Value";

constexpr auto FlibraryBackup = "FlibraryBackup";

constexpr auto FlibraryBackupVersion = "FlibraryBackupVersion";
constexpr auto FlibraryBackupVersionNumber = 6;
constexpr auto FlibraryUserData = "FlibraryUserData";

constexpr auto FlibraryDatabaseVersionNumber = 3;

constexpr int APP_FAILED = 1;
constexpr int RESTART_APP = 1234;
constexpr size_t MAX_LOG_SIZE = 10000;

constexpr auto OPDS_SERVER_NAME = "02bb69b1-003a-4892-85c7-7bcef3938565";
constexpr auto OPDS_SERVER_COMMAND_RESTART = "restart";
constexpr auto OPDS_SERVER_COMMAND_STOP = "stop";

constexpr auto BOOK = "Book:";

const auto INFO = []
{
	static constexpr char32_t info = 0x0001F6C8;
	return QString::fromUcs4(&info, 1);
}();

} // namespace HomeCompa::Flibrary::Constant
