#pragma once

class QByteArray;
class QString;

#define OPDS_GET_BOOKS_API_ITEMS_X_MACRO                          \
	OPDS_GET_BOOKS_API_ITEM(getConfig, )                          \
	OPDS_GET_BOOKS_API_ITEM(getSearchStats, search)               \
	OPDS_GET_BOOKS_API_ITEM(getSearchTitles, search)              \
	OPDS_GET_BOOKS_API_ITEM(getSearchAuthors, search)             \
	OPDS_GET_BOOKS_API_ITEM(getSearchSeries, search)              \
	OPDS_GET_BOOKS_API_ITEM(getSearchAuthorBooks, selectedItemID) \
	OPDS_GET_BOOKS_API_ITEM(getSearchSeriesBooks, selectedItemID) \
	OPDS_GET_BOOKS_API_ITEM(getBookForm, selectedItemID)

namespace HomeCompa::Opds
{

class IReactAppRequester // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IReactAppRequester() = default;

#define OPDS_GET_BOOKS_API_ITEM(NAME, _) virtual QByteArray NAME(const QString& value) const = 0;
	OPDS_GET_BOOKS_API_ITEMS_X_MACRO
#undef OPDS_GET_BOOKS_API_ITEM
};

}
