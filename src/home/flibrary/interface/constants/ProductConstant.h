#pragma once

#include <QString>

namespace HomeCompa::Flibrary::Constant
{

constexpr auto UI = "ui";

constexpr auto ITEM  = "Item";
constexpr auto TITLE = "Title";
constexpr auto VALUE = "Value";

constexpr auto FlibraryBackup = "FlibraryBackup";

constexpr auto FlibraryBackupVersion       = "FlibraryBackupVersion";
constexpr auto FlibraryBackupVersionNumber = 8;
constexpr auto FlibraryUserData            = "FlibraryUserData";

constexpr auto MinimumFlibraryDatabaseVersionNumber = 11;
constexpr auto FlibraryDatabaseVersionNumber        = 13;

constexpr size_t MAX_LOG_SIZE = 10000;

constexpr auto OPDS_SERVER_NAME            = "02bb69b1-003a-4892-85c7-7bcef3938565";
constexpr auto OPDS_SERVER_COMMAND_RESTART = "restart";
constexpr auto OPDS_SERVER_COMMAND_STOP    = "stop";

constexpr auto BOOK = "Book:";

constexpr auto BOOK_HASH_MIME_DATA_TYPE = "FLIBRARY_BOOK_HASH_MIME_DATA_TYPE";

constexpr auto BACKUP_FILE_EXT = ".flibk";

} // namespace HomeCompa::Flibrary::Constant
