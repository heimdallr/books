#pragma once

constexpr wchar_t COMMENT_START = '#';
constexpr wchar_t DATE_SEPARATOR = '-';
constexpr wchar_t FIELDS_SEPARATOR = '\x04';
constexpr wchar_t GENRE_SEPARATOR = '|';
constexpr wchar_t INI_SEPARATOR = '=';
constexpr wchar_t LIST_SEPARATOR = ':';
constexpr wchar_t NAMES_SEPARATOR = ',';

constexpr auto COLLECTION_INFO = L"collection.info";
constexpr auto DB_CREATE_SCRIPT = L"db_create_script";
constexpr auto DB_PATH = L"db_path";
constexpr auto DB_UPDATE_SCRIPT = L"db_update_script";
constexpr auto DEFAULT_DB_CREATE_SCRIPT = L"CreateCollection.sql";
constexpr auto DEFAULT_DB_UPDATE_SCRIPT = L"UpdateCollection.sql";
constexpr auto DEFAULT_GENRES = L"genres.lst";
constexpr auto GENRES = L"genres";
constexpr auto LANGUAGES_MAPPING = L"languages_mapping";
constexpr auto DEFAULT_LANGUAGES_MAPPING = L"LanguagesMapping.json";
constexpr auto INI_EXT = L"ini";
constexpr auto INPX_EXT = L".inpx";
constexpr auto INPX_FOLDER = L"inpx_folder";
constexpr auto MHL_TRIGGERS_ON = L"mhl_triggers_on";
constexpr auto VERSION_INFO = L"version.info";
constexpr auto ZIP = L"zip";
constexpr auto AUTHOR_UNKNOWN = L"Unknown author";

constexpr const char* SCHEMA_VERSION_VALUE = "{FEC8CB6F-300A-4b92-86D1-7B40867F782B}";

constexpr auto INP_EXT = L".inp";

constexpr size_t LOG_INTERVAL = 10000;

constexpr auto GET_MAX_ID_QUERY = "select coalesce(max(m), 0) from ("
								  "      select max(BookID)    m from Books "
								  "union select max(AuthorID)  m from Authors "
								  "union select max(SeriesID)  m from Series "
								  "union select max(KeywordID) m from Keywords "
								  "union select max(FolderID)  m from Folders "
								  ")";

static constexpr auto IS_DELETED_UPDATE_STATEMENTS = R"(
update %1 set IsDeleted = not exists (
	select 42 from Books b 
		%2
		left join Books_User bu on bu.BookID = b.BookID 
		where coalesce(bu.IsDeleted, b.IsDeleted, 0) = 0 
			and %3
	)
	%4
)";

static constexpr std::tuple<const char* /*table*/, const char* /*join*/, const char* /*where and condition*/, const char* /*additional condition*/> IS_DELETED_UPDATE_ARGS[] {
	{     "Authors",      "join Author_List l on l.BookID = b.BookID",    "l.AuthorID = Authors.AuthorID",                                                                               "" },
	{     "Folders",											   "",    "b.FolderID = Folders.FolderID",																			   "" },
	{	  "Genres",       "join Genre_List l on l.BookID = b.BookID",   "l.GenreCode = Genres.GenreCode", "and not exists (select 42 from Genres g where g.ParentCode = Genres.GenreCode)" },
	{ "Groups_User", "join Groups_List_User l on l.BookID = b.BookID",  "l.GroupID = Groups_User.GroupID",                                                                               "" },
	{    "Keywords",     "join Keyword_List l on l.BookID = b.BookID", "l.KeywordID = Keywords.KeywordID",                                                                               "" },
	{	  "Series",      "join Series_List l on l.BookID = b.BookID",     "l.SeriesID = Series.SeriesID",                                                                               "" },
	{     "Updates",											   "",    "b.UpdateID = Updates.UpdateID",  "and not exists (select 42 from Updates u where u.ParentID = Updates.UpdateID)" },
};
