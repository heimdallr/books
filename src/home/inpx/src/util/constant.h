#pragma once

[[maybe_unused]] constexpr wchar_t COMMENT_START     = '#';
[[maybe_unused]] constexpr wchar_t DATE_SEPARATOR   = '-';
[[maybe_unused]] constexpr wchar_t FIELDS_SEPARATOR = '\x04';
[[maybe_unused]] constexpr wchar_t GENRE_SEPARATOR  = '|';
[[maybe_unused]] constexpr wchar_t INI_SEPARATOR    = '=';
[[maybe_unused]] constexpr wchar_t LIST_SEPARATOR   = ':';
[[maybe_unused]] constexpr wchar_t NAMES_SEPARATOR  = ',';

[[maybe_unused]] constexpr const wchar_t * COLLECTION_INFO          = L"collection.info";
[[maybe_unused]] constexpr const wchar_t * DB_CREATE_SCRIPT         = L"db_create_script";
[[maybe_unused]] constexpr const wchar_t * DB_PATH                  = L"db_path";
[[maybe_unused]] constexpr const wchar_t * DB_UPDATE_SCRIPT         = L"db_update_script";
[[maybe_unused]] constexpr const wchar_t * DEFAULT_DB_CREATE_SCRIPT = L"db_create.sql";
[[maybe_unused]] constexpr const wchar_t * DEFAULT_DB_PATH          = L"db.hlc2";
[[maybe_unused]] constexpr const wchar_t * DEFAULT_DB_UPDATE_SCRIPT = L"db_update.sql";
[[maybe_unused]] constexpr const wchar_t * DEFAULT_GENRES           = L"genres.ini";
[[maybe_unused]] constexpr const wchar_t * DEFAULT_INPX             = L"db.inpx";
[[maybe_unused]] constexpr const wchar_t * GENRES                   = L"genres";
[[maybe_unused]] constexpr const wchar_t * INI_EXT                  = L"ini";
[[maybe_unused]] constexpr const wchar_t * INPX                     = L"inpx";
[[maybe_unused]] constexpr const wchar_t * MHL_TRIGGERS_ON          = L"mhl_triggers_on";
[[maybe_unused]] constexpr const wchar_t * VERSION_INFO             = L"version.info";
[[maybe_unused]] constexpr const wchar_t * ZIP                      = L"zip";

[[maybe_unused]] constexpr const char * SCHEMA_VERSION_VALUE = "{FEC8CB6F-300A-4b92-86D1-7B40867F782B}";

[[maybe_unused]] constexpr std::wstring_view INP_EXT = L".inp";
[[maybe_unused]] constexpr std::wstring_view UNKNOWN = L"unknown_root";
[[maybe_unused]] constexpr std::wstring_view DATE_ADDED_CODE = L"date_added_code_root";
[[maybe_unused]] constexpr std::wstring_view NO_DATE_SPECIFIED = L"No date specified";

[[maybe_unused]] constexpr size_t LOG_INTERVAL = 10000;

[[maybe_unused]] constexpr uint32_t PROP_CLASS_SYSTEM     = 0x10000000;
[[maybe_unused]] constexpr uint32_t PROP_CLASS_COLLECTION = 0x20000000;
[[maybe_unused]] constexpr uint32_t PROP_CLASS_BOTH       = PROP_CLASS_SYSTEM | PROP_CLASS_COLLECTION;
[[maybe_unused]] constexpr uint32_t PROP_CLASS_MASK       = 0xF0000000;

[[maybe_unused]] constexpr uint32_t PROP_TYPE_INTEGER  = 0x00010000;
[[maybe_unused]] constexpr uint32_t PROP_TYPE_DATETIME = 0x00020000;
[[maybe_unused]] constexpr uint32_t PROP_TYPE_BOOLEAN  = 0x00030000;
[[maybe_unused]] constexpr uint32_t PROP_TYPE_STRING   = 0x00040000;
[[maybe_unused]] constexpr uint32_t PROP_TYPE_MASK     = 0x0FFF0000;

[[maybe_unused]] constexpr uint32_t PROP_ID               = PROP_CLASS_SYSTEM     | PROP_TYPE_INTEGER  | 0x0000;
[[maybe_unused]] constexpr uint32_t PROP_DATAFILE         = PROP_CLASS_SYSTEM     | PROP_TYPE_STRING   | 0x0001;
[[maybe_unused]] constexpr uint32_t PROP_CODE             = PROP_CLASS_BOTH       | PROP_TYPE_INTEGER  | 0x0002;
[[maybe_unused]] constexpr uint32_t PROP_DISPLAYNAME      = PROP_CLASS_SYSTEM     | PROP_TYPE_STRING   | 0x0003;
[[maybe_unused]] constexpr uint32_t PROP_ROOTFOLDER       = PROP_CLASS_SYSTEM     | PROP_TYPE_STRING   | 0x0004;
[[maybe_unused]] constexpr uint32_t PROP_LIBUSER          = PROP_CLASS_SYSTEM     | PROP_TYPE_STRING   | 0x0005;
[[maybe_unused]] constexpr uint32_t PROP_LIBPASSWORD      = PROP_CLASS_SYSTEM     | PROP_TYPE_STRING   | 0x0006;
[[maybe_unused]] constexpr uint32_t PROP_URL              = PROP_CLASS_BOTH       | PROP_TYPE_STRING   | 0x0007;
[[maybe_unused]] constexpr uint32_t PROP_CONNECTIONSCRIPT = PROP_CLASS_BOTH       | PROP_TYPE_STRING   | 0x0008;
[[maybe_unused]] constexpr uint32_t PROP_DATAVERSION      = PROP_CLASS_BOTH       | PROP_TYPE_INTEGER  | 0x0009;
[[maybe_unused]] constexpr uint32_t PROP_NOTES            = PROP_CLASS_COLLECTION | PROP_TYPE_STRING   | 0x000A;
[[maybe_unused]] constexpr uint32_t PROP_CREATIONDATE     = PROP_CLASS_COLLECTION | PROP_TYPE_DATETIME | 0x000B;
[[maybe_unused]] constexpr uint32_t PROP_SCHEMA_VERSION   = PROP_CLASS_COLLECTION | PROP_TYPE_STRING   | 0x000C;

[[maybe_unused]] constexpr uint32_t PROP_LAST_AUTHOR      = PROP_CLASS_COLLECTION | PROP_TYPE_STRING   | 0x000D;
[[maybe_unused]] constexpr uint32_t PROP_LAST_AUTHOR_BOOK = PROP_CLASS_COLLECTION | PROP_TYPE_INTEGER  | 0x000F;

[[maybe_unused]] constexpr uint32_t PROP_LAST_SERIES      = PROP_CLASS_COLLECTION | PROP_TYPE_STRING   | 0x0010;
[[maybe_unused]] constexpr uint32_t PROP_LAST_SERIES_BOOK = PROP_CLASS_COLLECTION | PROP_TYPE_INTEGER  | 0x0011;

constexpr uint32_t g_collectionInfoSettings[]
{
	0, 0, PROP_CODE, PROP_NOTES,
};
