#pragma once

[[maybe_unused]] constexpr wchar_t COMMENT_START     = '#';
[[maybe_unused]] constexpr wchar_t DATE_SEPARATOR   = '-';
[[maybe_unused]] constexpr wchar_t FIELDS_SEPARATOR = '\x04';
[[maybe_unused]] constexpr wchar_t GENRE_SEPARATOR  = '|';
[[maybe_unused]] constexpr wchar_t INI_SEPARATOR    = '=';
[[maybe_unused]] constexpr wchar_t LIST_SEPARATOR   = ':';
[[maybe_unused]] constexpr wchar_t NAMES_SEPARATOR  = ',';

[[maybe_unused]] constexpr auto COLLECTION_INFO          = L"collection.info";
[[maybe_unused]] constexpr auto DB_CREATE_SCRIPT         = L"db_create_script";
[[maybe_unused]] constexpr auto DB_PATH                  = L"db_path";
[[maybe_unused]] constexpr auto DB_UPDATE_SCRIPT         = L"db_update_script";
[[maybe_unused]] constexpr auto DEFAULT_DB_CREATE_SCRIPT = L"CreateCollection.sql";
[[maybe_unused]] constexpr auto DEFAULT_DB_UPDATE_SCRIPT = L"UpdateCollection.sql";
[[maybe_unused]] constexpr auto DEFAULT_GENRES           = L"genres.lst";
[[maybe_unused]] constexpr auto GENRES                   = L"genres";
[[maybe_unused]] constexpr auto INI_EXT                  = L"ini";
[[maybe_unused]] constexpr auto INPX_EXT                 = L".inpx";
[[maybe_unused]] constexpr auto INPX_FOLDER              = L"inpx_folder";
[[maybe_unused]] constexpr auto MHL_TRIGGERS_ON          = L"mhl_triggers_on";
[[maybe_unused]] constexpr auto VERSION_INFO             = L"version.info";
[[maybe_unused]] constexpr auto ZIP                      = L"zip";

[[maybe_unused]] constexpr const char * SCHEMA_VERSION_VALUE = "{FEC8CB6F-300A-4b92-86D1-7B40867F782B}";

[[maybe_unused]] constexpr auto INP_EXT = L".inp";
[[maybe_unused]] constexpr auto DATE_ADDED_CODE = L"date_added_code_root";
[[maybe_unused]] constexpr auto NO_DATE_SPECIFIED = L"No date specified";

[[maybe_unused]] constexpr size_t LOG_INTERVAL = 10000;

[[maybe_unused]] constexpr auto GET_MAX_ID_QUERY =
	"select coalesce(max(m), 0) from ("
	"      select max(BookID)    m from Books "
	"union select max(AuthorID)  m from Authors "
	"union select max(SeriesID)  m from Series "
	"union select max(KeywordID) m from Keywords "
	"union select max(FolderID)  m from Folders "
	")";
