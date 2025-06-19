#pragma once

#include <unordered_map>

class QByteArray;
class QString;

#define OPDS_GET_BOOKS_API_ITEMS_X_MACRO          \
	OPDS_GET_BOOKS_API_ITEM(getConfig)            \
	OPDS_GET_BOOKS_API_ITEM(getSearchStats)       \
	OPDS_GET_BOOKS_API_ITEM(getSearchTitles)      \
	OPDS_GET_BOOKS_API_ITEM(getSearchAuthors)     \
	OPDS_GET_BOOKS_API_ITEM(getSearchSeries)      \
	OPDS_GET_BOOKS_API_ITEM(getSearchAuthorBooks) \
	OPDS_GET_BOOKS_API_ITEM(getSearchSeriesBooks) \
	OPDS_GET_BOOKS_API_ITEM(getBookForm)

namespace HomeCompa::Opds
{

class IReactAppRequester // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Parameters = std::unordered_map<QString, QString>;

public:
	virtual ~IReactAppRequester() = default;

#define OPDS_GET_BOOKS_API_ITEM(NAME) virtual QByteArray NAME(const Parameters& value) const = 0;
	OPDS_GET_BOOKS_API_ITEMS_X_MACRO
#undef OPDS_GET_BOOKS_API_ITEM
};

}
