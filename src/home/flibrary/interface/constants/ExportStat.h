#pragma once

namespace HomeCompa::Flibrary::ExportStat {

constexpr auto INSERT_QUERY = "insert into Export_List(BookID, ExportType, CreatedAt) values(?, ?, datetime(CURRENT_TIMESTAMP, 'localtime'))";

#define EXPORT_STAT_TYPE_ITEMS_X_MACRO \
		EXPORT_STAT_TYPE_ITEM(Read)    \
		EXPORT_STAT_TYPE_ITEM(AsIs)    \
		EXPORT_STAT_TYPE_ITEM(Archive) \
		EXPORT_STAT_TYPE_ITEM(Script)  \
		EXPORT_STAT_TYPE_ITEM(Inpx)

enum class Type
{
#define EXPORT_STAT_TYPE_ITEM(NAME) NAME,
		EXPORT_STAT_TYPE_ITEMS_X_MACRO
#undef	EXPORT_STAT_TYPE_ITEM
};

constexpr const char * NAMES[]
{
#define EXPORT_STAT_TYPE_ITEM(NAME) #NAME,
		EXPORT_STAT_TYPE_ITEMS_X_MACRO
#undef	EXPORT_STAT_TYPE_ITEM
};

constexpr auto UNKNOWN_TYPE = "Unknown type";

inline const char * GetName(const Type type)
{
	const auto index = static_cast<size_t>(type);
	return index < std::size(NAMES) ? NAMES[index] : UNKNOWN_TYPE;
}

}
