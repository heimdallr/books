#pragma once

#include <QString>

class QByteArray;

#define OPDS_ROOT_ITEMS_X_MACRO \
	OPDS_ROOT_ITEM(Authors)     \
	OPDS_ROOT_ITEM(Series)      \
	OPDS_ROOT_ITEM(Genres)      \
	OPDS_ROOT_ITEM(Keywords)    \
	OPDS_ROOT_ITEM(Archives)    \
	OPDS_ROOT_ITEM(Groups)

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

class IRequester // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IRequester() = default;

	virtual QByteArray GetRoot(const QString& root, const QString& self) const = 0;
	virtual QByteArray GetBookInfo(const QString& root, const QString& self, const QString& bookId) const = 0;
	virtual QByteArray GetCover(const QString& root, const QString& self, const QString& bookId) const = 0;
	virtual QByteArray GetCoverThumbnail(const QString& root, const QString& self, const QString& bookId) const = 0;
	virtual std::pair<QString, QByteArray> GetBook(const QString& root, const QString& self, const QString& bookId, bool transliterate) const = 0;
	virtual std::pair<QString, QByteArray> GetBookZip(const QString& root, const QString& self, const QString& bookId, bool transliterate) const = 0;
	virtual QByteArray GetBookText(const QString& root, const QString& bookId) const = 0;
	virtual QByteArray Search(const QString& root, const QString& self, const QString& searchTerms, const QString& start = {}) const = 0;

#define OPDS_ROOT_ITEM(NAME) virtual QByteArray Get##NAME##Navigation(const QString& root, const QString& self, const QString& value) const = 0;
	OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM

#define OPDS_ROOT_ITEM(NAME) virtual QByteArray Get##NAME##Authors(const QString& root, const QString& self, const QString& navigationId, const QString& value) const = 0;
	OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM

#define OPDS_ROOT_ITEM(NAME) virtual QByteArray Get##NAME##AuthorBooks(const QString& root, const QString& self, const QString& navigationId, const QString& authorId, const QString& value) const = 0;
	OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM

#define OPDS_GET_BOOKS_API_ITEM(NAME, _) virtual QByteArray NAME(const QString& value) const = 0;
	OPDS_GET_BOOKS_API_ITEMS_X_MACRO
#undef OPDS_GET_BOOKS_API_ITEM
};

} // namespace HomeCompa::Opds
